/////////////////////////////////////////////////////////
// FILE:		SaperaGigE.cpp
// PROJECT:		Teledyne DALSA Micro-Manager Glue Library
//-------------------------------------------------------
// AUTHOR: Robert Frazee, rfraze1@lsu.edu

#include "SaperaGigE.h"
#include "../MMDevice/ModuleInterface.h"
#include "stdio.h"
#include "conio.h"
#include "math.h"
#include <string>
#include <functional>

using namespace std;

const char* g_CameraName = "SaperaGigE";
const char* g_PixelType_8bit = "8bit";
const char* g_PixelType_10bit = "10bit";
const char* g_PixelType_12bit = "12bit";

///////////////////////////////////////////////////////////////////////////////
// Exported MMDevice API
///////////////////////////////////////////////////////////////////////////////

/**
 * List all supported hardware devices here
 */
MODULE_API void InitializeModuleData()
{
   RegisterDevice(g_CameraName, MM::CameraDevice, "Sapera GigE Camera Device");
}

MODULE_API MM::Device* CreateDevice(const char* deviceName)
{
   if (deviceName == 0)
      return 0;

   // decide which device class to create based on the deviceName parameter
   if (strcmp(deviceName, g_CameraName) == 0)
   {
      // create camera
      return new SaperaGigE();
   }

   // ...supplied name not recognized
   // to heck with it, return a device anyway
   return new SaperaGigE();
}

MODULE_API void DeleteDevice(MM::Device* pDevice)
{
   delete pDevice;
}

///////////////////////////////////////////////////////////////////////////////
// SaperaGigE implementation
// ~~~~~~~~~~~~~~~~~~~~~~~

/**
* SaperaGigE constructor.
* Setup default all variables and create device properties required to exist
* before intialization. In this case, no such properties were required. All
* properties will be created in the Initialize() method.
*
* As a general guideline Micro-Manager devices do not access hardware in the
* the constructor. We should do as little as possible in the constructor and
* perform most of the initialization in the Initialize() method.
*/
SaperaGigE::SaperaGigE() :
   binning_ (1),
   bytesPerPixel_(1),
   bitsPerPixel_(8),
   initialized_(false),
   roiX_(0),
   roiY_(0),
   thd_(0),
   sequenceRunning_(false),
   SapFormatBytes_(1)
{
   // call the base class method to set-up default error codes/messages
   InitializeDefaultErrorMessages();

   // Description property
   int ret = CreateProperty(MM::g_Keyword_Description, "Sapera GigE Camera Adapter", MM::String, true);
   assert(ret == DEVICE_OK);

   // camera type pre-initialization property

}

/**
* SaperaGigE destructor.
* If this device used as intended within the Micro-Manager system,
* Shutdown() will be always called before the destructor. But in any case
* we need to make sure that all resources are properly released even if
* Shutdown() was not called.
*/
SaperaGigE::~SaperaGigE()
{
   if (initialized_)
      Shutdown();
}

/**
* Obtains device name.
* Required by the MM::Device API.
*/
void SaperaGigE::GetName(char* name) const
{
   // We just return the name we use for referring to this
   // device adapter.
   CDeviceUtils::CopyLimitedString(name, g_CameraName);
}

/**
* Intializes the hardware.
* Typically we access and initialize hardware at this point.
* Device properties are typically created here as well.
* Required by the MM::Device API.
*/
int SaperaGigE::Initialize()
{
   if (initialized_)
      return DEVICE_OK;

   CPropertyAction* pAct;

   // Sapera++ library stuff
   int serverCount = 0;
   if (SapManager::DetectAllServers(SapManager::DetectServerAll))
   {
	   serverCount = SapManager::GetServerCount();
   }
   else
   {
	   LogMessage("No CameraLink camera servers detected", false);
	   return DEVICE_NATIVE_MODULE_FAILED;
   }

   char serverName[CORSERVER_MAX_STRLEN];
   for (int serverIndex = 0; serverIndex < serverCount; serverIndex++)
   {
	   if (SapManager::GetResourceCount(serverIndex, SapManager::ResourceAcqDevice) != 0)
	   {
		   // Get Server Name Value
		   SapManager::GetServerName(serverIndex, serverName, sizeof(serverName));
		   acqDeviceList_.push_back(serverName);
	   }
   }

   if (acqDeviceList_.size() == 0)
   {
	   ErrorBox("Initialization Error", "No servers!");
	   return DEVICE_NATIVE_MODULE_FAILED;
   }

   int ret = CreateProperty(g_CameraServerNameProperty, acqDeviceList_[0].c_str(), MM::String, false, 0, false);
   assert(ret == DEVICE_OK);

   ret = SetAllowedValues(g_CameraServerNameProperty, acqDeviceList_);
   assert(ret == DEVICE_OK);

   // create live video thread
   thd_ = new SequenceThread(this);

   SapLocation loc_(acqDeviceList_[0].c_str());
   //if(SapManager::GetResourceCount(acqServerName_, SapManager::ResourceAcqDevice) > 0)
   //{
	   AcqDevice_ = SapAcqDevice(loc_, false);
	   Buffers_ = SapBufferWithTrash(2, &AcqDevice_);
	   AcqDeviceToBuf_ = SapAcqDeviceToBuf(&AcqDevice_, &Buffers_);
	   Xfer_ = &AcqDeviceToBuf_;

	   if(!AcqDevice_.Create())
	   {
		   ret = FreeHandles();
		   if (ret != DEVICE_OK)
		   {
			   //SapManager::DisplayMessage("Failed to FreeHandles during Acq_.Create() for ResourceAcqDevice");
			   return ret;
		   }
		   //SapManager::DisplayMessage("Failed to create Acq_ for ResourceAcqDevice");
		   return DEVICE_INVALID_INPUT_PARAM;
	   }
   //}
   //SapManager::DisplayMessage("(Sapera app)GetResourceCount for ResourceAcqDevice done");
   //SapManager::DisplayMessage("(Sapera app)Creating Buffers_");
   if(!Buffers_.Create())
	{
		ret = FreeHandles();
		if (ret != DEVICE_OK)
			return ret;
		return DEVICE_NATIVE_MODULE_FAILED;
	}
   //SapManager::DisplayMessage("(Sapera app)Creating Xfer_");
   if(Xfer_ && !Xfer_->Create())
	{
		//SapManager::DisplayMessage("Xfer_ creation failed");
		ret = FreeHandles();
		if (ret != DEVICE_OK)
			return ret;
		return DEVICE_NATIVE_MODULE_FAILED;
	}
	//SapManager::DisplayMessage("(Sapera app)Starting Xfer");
	//Start continuous grab
	//Xfer_->Grab();
	//SapManager::DisplayMessage("(Sapera app)Sapera Initialization for SaperaGigE complete");


   // set property list
   // -----------------

   // binning
   pAct = new CPropertyAction(this, &SaperaGigE::OnBinning);
   //@TODO: Check what the actual binning value is and set that
   // For now, set binning to 1 for MM and set that on the camera later
   ret = CreateProperty(MM::g_Keyword_Binning, "1", MM::Integer, false, pAct);
   assert(ret == DEVICE_OK);

   vector<string> binningValues;
   binningValues.push_back("1");
   binningValues.push_back("2");
   binningValues.push_back("4");

   ret = SetAllowedValues(MM::g_Keyword_Binning, binningValues);
   assert(ret == DEVICE_OK);

	// synchronize bit depth with camera

	char acqFormat[10];
	AcqDevice_.GetFeatureValue("PixelFormat", acqFormat, sizeof(acqFormat));
	if(strcmp(acqFormat, "Mono8") == 0)
	{
		// Setup Micro-Manager for 8bit pixels
		SapFormatBytes_ = 1;
		bitsPerPixel_ = 8;
		bytesPerPixel_ = 1;
		//resize the SapBuffer
		int ret = SapBufferReformat(SapFormatMono8, "Mono8");
		if(ret != DEVICE_OK)
		{
			return ret;
		}
		ResizeImageBuffer();
		pAct = new CPropertyAction (this, &SaperaGigE::OnPixelType);
		ret = CreateProperty(MM::g_Keyword_PixelType, g_PixelType_8bit, MM::String, false, pAct);
		assert(ret == DEVICE_OK);
	}
	if(strcmp(acqFormat, "Mono10") == 0)
	{
		// Setup Micro-Manager for 8bit pixels
		SapFormatBytes_ = 2;
		bitsPerPixel_ = 10;
		bytesPerPixel_ = 2;
		//resize the SapBuffer
		int ret = SapBufferReformat(SapFormatMono10, "Mono10");
		if(ret != DEVICE_OK)
		{
			return ret;
		}
		ResizeImageBuffer();
		pAct = new CPropertyAction (this, &SaperaGigE::OnPixelType);
		ret = CreateProperty(MM::g_Keyword_PixelType, g_PixelType_10bit, MM::String, false, pAct);
		assert(ret == DEVICE_OK);
	}

	// pixel type
    vector<string> pixelTypeValues;
   pixelTypeValues.push_back(g_PixelType_8bit);
   pixelTypeValues.push_back(g_PixelType_10bit);
      
   ret = SetAllowedValues(MM::g_Keyword_PixelType, pixelTypeValues);
   assert(ret == DEVICE_OK);

   // Set Binning to 1
   if(!AcqDevice_.SetFeatureValue("BinningVertical", 1))
	   return DEVICE_ERR;
   if(!AcqDevice_.SetFeatureValue("BinningHorizontal", 1))
	   return DEVICE_ERR;

   // Device information
   for (auto const& x : deviceInfoFeaturesStr)
   {
	   BOOL isAvailable;
	   AcqDevice_.IsFeatureAvailable(x.second.c_str(), &isAvailable);
	   if (!isAvailable)
		   continue;
	   char value[MM::MaxStrLength];
	   if (!AcqDevice_.GetFeatureValue(x.second.c_str(), value, sizeof(value)))
		   return DEVICE_ERR;

	   string s = value;
	   ret = CreateProperty(x.first.c_str(), s.c_str(), MM::String, true);
	   assert(ret == DEVICE_OK);
   }

   // Device information
   for (auto const& x : deviceInfoFeaturesInt)
   {
	   BOOL isAvailable;
	   AcqDevice_.IsFeatureAvailable(x.second.c_str(), &isAvailable);
	   if (!isAvailable)
		   continue;
	   UINT32 value;
	   if (!AcqDevice_.GetFeatureValue(x.second.c_str(), &value))
		   return DEVICE_ERR;

	   string s = std::to_string(value);
	   ret = CreateProperty(x.first.c_str(), s.c_str(), MM::Integer, true);
	   assert(ret == DEVICE_OK);
   }

   // Create feature
   SapFeature feature(loc_);
   if (!feature.Create())
	   return DEVICE_ERR;
   double low = 0.0;
   double high = 0.0;

   // Set up gain
   pAct = new CPropertyAction(this, &SaperaGigE::OnGain);
   ret = CreateProperty(MM::g_Keyword_Gain, "1.0", MM::Float, false, pAct);
   assert(ret == DEVICE_OK);
   if(!AcqDevice_.SetFeatureValue("Gain", 1.0))
	   return DEVICE_ERR;
   AcqDevice_.GetFeatureInfo("Gain", &feature);
   feature.GetMax(&high);
   feature.GetMin(&low);
   SetPropertyLimits(MM::g_Keyword_Gain, low, high);

   // Set up exposure
   pAct = new CPropertyAction(this, &SaperaGigE::OnExposure);
   ret = CreateProperty(MM::g_Keyword_Exposure, "1.0", MM::Float, false, pAct);
   assert(ret == DEVICE_OK);
   if (!AcqDevice_.SetFeatureValue("ExposureTime", 1000.0)) // us
	   return DEVICE_ERR;
   AcqDevice_.GetFeatureInfo("ExposureTime", &feature);
   feature.GetMax(&high); // us
   feature.GetMin(&low); // us
   SetPropertyLimits(MM::g_Keyword_Exposure, low / 1000., high / 1000.);

   // Set up temperature
   pAct = new CPropertyAction(this, &SaperaGigE::OnTemperature);
   ret = CreateProperty("Device Temperature", "-1.0", MM::Float, true, pAct);
   assert(ret == DEVICE_OK);
   
   // synchronize all properties
   // --------------------------
   ret = UpdateStatus();
   if (ret != DEVICE_OK)
      return ret;

   // setup the buffer
   // ----------------
   ret = ResizeImageBuffer();
   if (ret != DEVICE_OK)
      return ret;

   initialized_ = true;
   return DEVICE_OK;
}

/**
* Shuts down (unloads) the device.
* Ideally this method will completely unload the device and release all resources.
* Shutdown() may be called multiple times in a row.
* Required by the MM::Device API.
*/
int SaperaGigE::Shutdown()
{
	if(!initialized_)
		return DEVICE_OK;
	initialized_ = false;
	Xfer_->Freeze();
	if(!Xfer_->Wait(5000))
		return DEVICE_NATIVE_MODULE_FAILED;
	int ret;
	ret = FreeHandles();
	if(ret != DEVICE_OK)
		return ret;
	return DEVICE_OK;
}

/**
* Frees Sapera buffers and such
*/
int SaperaGigE::FreeHandles()
{
	if(Xfer_ && *Xfer_ && !Xfer_->Destroy()) return DEVICE_ERR;
	if(!Buffers_.Destroy()) return DEVICE_ERR;
	if(!Acq_.Destroy()) return DEVICE_ERR;
	if(!AcqDevice_.Destroy()) return DEVICE_ERR;
	return DEVICE_OK;
}

int SaperaGigE::ErrorBox(std::string text, std::string caption)
{
	return MessageBox(NULL, (LPCWSTR)caption.c_str(), (LPCWSTR)text.c_str(), (MB_ICONERROR | MB_OK));
}

/**
* Performs exposure and grabs a single image.
* This function blocks during the actual exposure and returns immediately afterwards 
* Required by the MM::Camera API.
*/
int SaperaGigE::SnapImage()
{
	// This will always be false, as no sequences will ever run
	if(sequenceRunning_)
		return DEVICE_CAMERA_BUSY_ACQUIRING;
	// Start image capture
	if(!Xfer_->Snap(1))
	{
		return DEVICE_ERR;
	}
	// Wait for either the capture to finish or 2.5 seconds, whichever is first
	if(!Xfer_->Wait(2500))
	{
		return DEVICE_ERR;
	}
	return DEVICE_OK;
}

/**
* Returns pixel data.
* Required by the MM::Camera API.
* The calling program will assume the size of the buffer based on the values
* obtained from GetImageBufferSize(), which in turn should be consistent with
* values returned by GetImageWidth(), GetImageHight() and GetImageBytesPerPixel().
* The calling program allso assumes that camera never changes the size of
* the pixel buffer on its own. In other words, the buffer can change only if
* appropriate properties are set (such as binning, pixel type, etc.)
*/
const unsigned char* SaperaGigE::GetImageBuffer()
{
	// Put Sapera buffer into Micro-Manager Buffer
	Buffers_.ReadRect(roiX_, roiY_, img_.Width(), img_.Height(), const_cast<unsigned char*>(img_.GetPixels()));
	// Return location of the Micro-Manager Buffer
	return const_cast<unsigned char*>(img_.GetPixels());
}

/**
* Returns image buffer X-size in pixels.
* Required by the MM::Camera API.
*/
unsigned SaperaGigE::GetImageWidth() const
{
   return img_.Width();
}

/**
* Returns image buffer Y-size in pixels.
* Required by the MM::Camera API.
*/
unsigned SaperaGigE::GetImageHeight() const
{
   return img_.Height();
}

/**
* Returns image buffer pixel depth in bytes.
* Required by the MM::Camera API.
*/
unsigned SaperaGigE::GetImageBytesPerPixel() const
{
   return img_.Depth();
} 

/**
* Returns the bit depth (dynamic range) of the pixel.
* This does not affect the buffer size, it just gives the client application
* a guideline on how to interpret pixel values.
* Required by the MM::Camera API.
*/
unsigned SaperaGigE::GetBitDepth() const
{
   return bitsPerPixel_;
}

/**
* Returns the size in bytes of the image buffer.
* Required by the MM::Camera API.
*/
long SaperaGigE::GetImageBufferSize() const
{
   return img_.Width() * img_.Height() * GetImageBytesPerPixel();
}

/**
* Sets the camera Region Of Interest.
* Required by the MM::Camera API.
* This command will change the dimensions of the image.
* Depending on the hardware capabilities the camera may not be able to configure the
* exact dimensions requested - but should try do as close as possible.
* If the hardware does not have this capability the software should simulate the ROI by
* appropriately cropping each frame.
* This demo implementation ignores the position coordinates and just crops the buffer.
* @param x - top-left corner coordinate
* @param y - top-left corner coordinate
* @param xSize - width
* @param ySize - height
*/
int SaperaGigE::SetROI(unsigned x, unsigned y, unsigned xSize, unsigned ySize)
{
   if (xSize == 0 && ySize == 0)
   {
      // effectively clear ROI
      ResizeImageBuffer();
      roiX_ = 0;
      roiY_ = 0;
   }
   else
   {
      // apply ROI
      img_.Resize(xSize, ySize);
      roiX_ = x;
      roiY_ = y;
   }
   return DEVICE_OK;
}

/**
* Returns the actual dimensions of the current ROI.
* Required by the MM::Camera API.
*/
int SaperaGigE::GetROI(unsigned& x, unsigned& y, unsigned& xSize, unsigned& ySize)
{
   x = roiX_;
   y = roiY_;

   xSize = img_.Width();
   ySize = img_.Height();

   return DEVICE_OK;
}

/**
* Resets the Region of Interest to full frame.
* Required by the MM::Camera API.
*/
int SaperaGigE::ClearROI()
{
   ResizeImageBuffer();
   roiX_ = 0;
   roiY_ = 0;
      
   return DEVICE_OK;
}

/**
* Returns the current exposure setting in milliseconds.
* Required by the MM::Camera API.
*/
double SaperaGigE::GetExposure() const
{
   char buf[MM::MaxStrLength];
   int ret = GetProperty(MM::g_Keyword_Exposure, buf);
   if (ret != DEVICE_OK)
	   return 0.0;
   return atof(buf);
}

/**
* Sets exposure in milliseconds.
* Required by the MM::Camera API.
*/
void SaperaGigE::SetExposure(double exp)
{
   int ret = SetProperty(MM::g_Keyword_Exposure, std::to_string(exp).c_str());
 }

/**
* Returns the current binning factor.
* Required by the MM::Camera API.
*/
int SaperaGigE::GetBinning() const
{
   return binning_;
}

/**
* Sets binning factor.
* Required by the MM::Camera API.
*/
int SaperaGigE::SetBinning(int binF)
{
   return SetProperty(MM::g_Keyword_Binning, CDeviceUtils::ConvertToString(binF));
}

int SaperaGigE::PrepareSequenceAcqusition()
{
	return DEVICE_ERR;
}


/**
 * Required by the MM::Camera API
 * Please implement this yourself and do not rely on the base class implementation
 * The Base class implementation is deprecated and will be removed shortly
 */
int SaperaGigE::StartSequenceAcquisition(double interval_ms)
{
	//@TODO: Implement Sequence Acquisition
	return DEVICE_ERR;
	//int ret = StartSequenceAcquisition((long)(interval_ms/exposureMs_), interval_ms, true);
	//return ret;
}

/**                                                                       
* Stop and wait for the Sequence thread finished                                   
*/                                                                        
int SaperaGigE::StopSequenceAcquisition()                                     
{
	//@TODO: Implement Sequence Acquisition
	return DEVICE_ERR;
	/*thd_->Stop();
	thd_->wait();
	sequenceRunning_ = false;
	return DEVICE_OK;*/                                                      
} 

/**
* Simple implementation of Sequence Acquisition
* A sequence acquisition should run on its own thread and transport new images
* coming of the camera into the MMCore circular buffer.
*/
int SaperaGigE::StartSequenceAcquisition(long numImages, double interval_ms, bool stopOnOverflow)
{
	//@TODO: Implement Sequence Acquisition
	return DEVICE_ERR;
	/*if (sequenceRunning_)
	{
		return DEVICE_CAMERA_BUSY_ACQUIRING;
	}
	int ret = GetCoreCallback()->PrepareForAcq(this);
	if (ret != DEVICE_OK)
	{
		return ret;
	}
	sequenceRunning_ = true;
	thd_->SetLength(10);
	thd_->Start();
   return DEVICE_OK; */
}

/*
 * Inserts Image and MetaData into MMCore circular Buffer
 */
int SaperaGigE::InsertImage()
{
	//@TODO: Implement Sequence Acquisition
	return GetCoreCallback()->InsertImage(this, const_cast<unsigned char*>(img_.GetPixels()), GetImageWidth(), GetImageHeight(), GetImageBytesPerPixel());
}


bool SaperaGigE::IsCapturing() {
	//@TODO: Implement Sequence Acquisition
   return sequenceRunning_;
}


///////////////////////////////////////////////////////////////////////////////
// SaperaGigE Action handlers
///////////////////////////////////////////////////////////////////////////////

/**
* Handles "Binning" property.
*/
int SaperaGigE::OnBinning(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::AfterSet)
   {
      long binSize;
      pProp->Get(binSize);
      binning_ = (int)binSize;
	  if(!AcqDevice_.SetFeatureValue("BinningVertical", binning_))
		  return DEVICE_ERR;
	  if(!AcqDevice_.SetFeatureValue("BinningHorizontal", binning_))
		  return DEVICE_ERR;
      return ResizeImageBuffer();
   }
   else if (eAct == MM::BeforeGet)
   {
      pProp->Set((long)binning_);
   }

   return DEVICE_OK;
}

int SaperaGigE::OnTemperature(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		double temperature;
		if (!AcqDevice_.GetFeatureValue("DeviceTemperature", &temperature))
			return DEVICE_ERR;
		pProp->Set(temperature);
		return DEVICE_OK;
	}
	else if (eAct == MM::AfterSet)
	{
		return DEVICE_CAN_NOT_SET_PROPERTY;
	}

	return DEVICE_OK;
}

/**
* Handles "PixelType" property.
*/
int SaperaGigE::OnPixelType(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	//bytesPerPixel_ = 1;
	//ResizeImageBuffer();
	//return DEVICE_OK;
   if (eAct == MM::AfterSet)
   {
	  string val;
      pProp->Get(val);
      if (val.compare(g_PixelType_8bit) == 0)
	  {
		  if(SapFormatBytes_ != 1)
		  {
			  SapFormatBytes_ = 1;
			  bitsPerPixel_ = 8;
			  //resize the SapBuffer
			  int ret = SapBufferReformat(SapFormatMono8, "Mono8");
			  if(ret != DEVICE_OK)
			  {
				  return ret;
			  }
		  }
         bytesPerPixel_ = 1;
	  }
      else if (val.compare(g_PixelType_10bit) == 0)
	  {
		  if(SapFormatBytes_ != 2)
		  {
			  SapFormatBytes_ = 2;
			  bitsPerPixel_ = 10;
			  //resize the SapBuffer
			  int ret = SapBufferReformat(SapFormatMono16, "Mono10");
			  if(ret != DEVICE_OK)
			  {
				  return ret;
			  }
		  }
         bytesPerPixel_ = 2;
	  }
      else
         assert(false);

      ResizeImageBuffer();
   }
   else if (eAct == MM::BeforeGet)
   {
      if (bytesPerPixel_ == 1)
         pProp->Set(g_PixelType_8bit);
      else if (bytesPerPixel_ == 2)
         pProp->Set(g_PixelType_10bit);
      else
         assert(false); // this should never happen
   }

   return DEVICE_OK;
}

/**
* Handles "Gain" property.
*/
int SaperaGigE::OnGain(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	double gain = 1.;
	if (eAct == MM::AfterSet)
   {
      pProp->Get(gain);
	  AcqDevice_.SetFeatureValue("Gain", gain);
   }
   else if (eAct == MM::BeforeGet)
   {
		AcqDevice_.GetFeatureValue("Gain", &gain);
		pProp->Set(gain);
   }

   return DEVICE_OK;
}

int SaperaGigE::OnExposure(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	// note that GigE units of exposure are us; umanager uses ms
	if (eAct == MM::AfterSet)
	{
			double oldd = 0, newd = 0;
			AcqDevice_.GetFeatureValue("ExposureTime", &oldd); // us
			pProp->Get(newd);  // ms
			if (!AcqDevice_.SetFeatureValue("ExposureTime", newd * 1000.0)) // ms to us
			{
				pProp->Set(oldd / 1000.0);  // us to ms
				return DEVICE_INVALID_PROPERTY_VALUE;
			}
	}
	else if (eAct == MM::BeforeGet)
	{
			double d = 0;
			if (AcqDevice_.GetFeatureValue("ExposureTime", &d)) // us
				pProp->Set(d / 1000.0);
	}
	return DEVICE_OK;
}


///////////////////////////////////////////////////////////////////////////////
// Private SaperaGigE methods
///////////////////////////////////////////////////////////////////////////////

/**
* Sync internal image buffer size to the chosen property values.
*/
int SaperaGigE::ResizeImageBuffer()
{
   img_.Resize(IMAGE_WIDTH/binning_, IMAGE_HEIGHT/binning_, bytesPerPixel_);

   return DEVICE_OK;
}

/**
 * Generate an image with fixed value for all pixels
 */
void SaperaGigE::GenerateImage()
{
   const int maxValue = (1 << MAX_BIT_DEPTH) - 1; // max for the 12 bit camera
   const double maxExp = 1000;
   double step = maxValue/maxExp;
   unsigned char* pBuf = const_cast<unsigned char*>(img_.GetPixels());
   double exposureMs = GetExposure();
   memset(pBuf, (int) (step * max(exposureMs, maxExp)), img_.Height()*img_.Width()*img_.Depth());
}

/*
 * Reformat Sapera Buffer Object
 */
int SaperaGigE::SapBufferReformat(SapFormat format, const char * acqFormat)
{
	Xfer_->Destroy();
	AcqDevice_.SetFeatureValue("PixelFormat", acqFormat);
	Buffers_.Destroy();
	Buffers_ = SapBufferWithTrash(2, &AcqDevice_);
	Buffers_.SetFormat(format);
	AcqDeviceToBuf_ = SapAcqDeviceToBuf(&AcqDevice_, &Buffers_);
	Xfer_ = &AcqDeviceToBuf_;
	if(!Buffers_.Create())
	{
		//SapManager::DisplayMessage("Failed to recreate Buffer - SapBufferReformat");
		int ret = FreeHandles();
		if (ret != DEVICE_OK)
			return ret;
		return DEVICE_NATIVE_MODULE_FAILED;
	}
	if(Xfer_ && !Xfer_->Create())
	{
		//SapManager::DisplayMessage("Xfer_ recreation failed - SapBufferReformat");
		int ret = FreeHandles();
		if (ret != DEVICE_OK)
			return ret;
		return DEVICE_NATIVE_MODULE_FAILED;
	}
	return DEVICE_OK;
}

///////////////////////////////////////////////////////////////////////////////
// Threading methods
///////////////////////////////////////////////////////////////////////////////

int SequenceThread::svc()
{
	//SapManager::DisplayMessage("SequenceThread Start");
   long count(0);
   while (!stop_ )//&& count < numImages_)
   {
      /*int ret = camera_->SnapImage();
      if (ret != DEVICE_OK)
      {
		  //SapManager::DisplayMessage("SequenceThread Snap failed");
         camera_->StopSequenceAcquisition();
         return 1;
      }*/

      int ret = camera_->InsertImage();
      if (ret != DEVICE_OK)
      {
		 //SapManager::DisplayMessage("SequenceThread InsertFailed");
         camera_->StopSequenceAcquisition();
         return 1;
      }
      //count++;
   }
   //SapManager::DisplayMessage("SequenceThread End");
   return 0;
}