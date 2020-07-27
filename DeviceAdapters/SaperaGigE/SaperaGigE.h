///////////////////////////////////////////////////////////////////////////////
// FILE:          SaperaGigE.h
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   Skeleton code for the micro-manager camera adapter. Use it as
//                starting point for writing custom device adapters
//
// AUTHOR:        Nenad Amodaj, http://nenad.amodaj.com
//
// COPYRIGHT:     University of California, San Francisco, 2011
//
// LICENSE:       This file is distributed under the BSD license.
//                License text is included with the source distribution.
//
//                This file is distributed in the hope that it will be useful,
//                but WITHOUT ANY WARRANTY; without even the implied warranty
//                of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//
//                IN NO EVENT SHALL THE COPYRIGHT OWNER OR
//                CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
//                INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES.
//

#ifndef _SaperaGigE_H_
#define _SaperaGigE_H_

#include "DeviceBase.h"
#include "DeviceThreads.h"
#include "ImgBuffer.h"
#include "stdio.h"
#include "conio.h"
#include "math.h"
#include "SapClassBasic.h"
#include <string>

//////////////////////////////////////////////////////////////////////////////
// Error codes
//
#define ERR_UNKNOWN_MODE         102

class SequenceThread;

const std::map< std::string, std::string > deviceInfoFeaturesStr = {
    // information on device - use names shown in Sapera CamExpert
    {"Manufacturer Name", "DeviceVendorName"},
    {"Family Name", "DeviceFamilyName"},
    {"Model Name", "DeviceModelName"},
    {"Device Version", "DeviceVersion"},
    {"Manufacturer Info", "DeviceManufacturerInfo"},
    {"Manufacturer Part Number", "deviceManufacturerPartNumber"},
    {"Firmware Version", "DeviceFirmwareVersion"},
    {"Serial Number", "DeviceSerialNumber"},
    {"Device User ID", "DeviceUserID"},
    {"MAC Address", "deviceMacAddress"},
    {"SensorType", "sensorColorType"},
    {"SensorPixelCoding", "PixelCoding"},
    //{"SensorBlackLevel", "BlackLevel"},
    {"SensorPixelInput", "pixelSizeInput"},
    {"SensorShutterMode", "SensorShutterMode"},
    {"SensorBinningMode", "binningMode"}
};

const std::map< std::string, std::string > deviceInfoFeaturesInt = {
    {"SensorWidth", "SensorWidth"},
    {"SensorHeight", "SensorHeight"},
    //{"Sensor Pixel Size", "PixelSize"},
    //{"Horizontal Offset", "OffsetX"},
    //{"Vertical Offset", "OffsetY"},
};

//const std::map< std::string, SapFormat > pixelFormat = {
//    {"Mono8", SapFormatMono8},
//    {"Mono10", SapFormatMono10}
//};

const std::map< std::string, int > pixelBit = {
    {"8bit", 8},
    {"10bit", 10},
    {"12bit", 12}
};

const char* g_CameraServerNameProperty = "AcquisitionDevice";

class SaperaGigE : public CCameraBase<SaperaGigE>
{
public:
    SaperaGigE();
    ~SaperaGigE();

    // MMDevice API
    // ------------
    int Initialize();
    int Shutdown();

    void GetName(char* name) const;

    // SaperaGigE API
    // ------------
    int SnapImage();
    const unsigned char* GetImageBuffer();
    unsigned GetImageWidth() const;
    unsigned GetImageHeight() const;
    unsigned GetImageBytesPerPixel() const;
    unsigned GetBitDepth() const;
    long GetImageBufferSize() const;
    double GetExposure() const;
    void SetExposure(double exp);
    int SetROI(unsigned x, unsigned y, unsigned xSize, unsigned ySize);
    int GetROI(unsigned& x, unsigned& y, unsigned& xSize, unsigned& ySize);
    int ClearROI();
    int PrepareSequenceAcqusition();
    int StartSequenceAcquisition(double interval);
    int StartSequenceAcquisition(long numImages, double interval_ms, bool stopOnOverflow);
    int StopSequenceAcquisition();
    bool IsCapturing();
    int GetBinning() const;
    int SetBinning(int binSize);
    int IsExposureSequenceable(bool& seq) const { seq = false; return DEVICE_OK; }

    // action interface
    // ----------------
    int OnBinning(MM::PropertyBase* pProp, MM::ActionType eAct);
    int OnPixelSize(MM::PropertyBase* pProp, MM::ActionType eAct);
    int OnBlackLevel (MM::PropertyBase* pProp, MM::ActionType eAct);
    int OnOffsetX(MM::PropertyBase* pProp, MM::ActionType eAct);
    int OnOffsetY(MM::PropertyBase* pProp, MM::ActionType eAct);
    int OnWidth(MM::PropertyBase* pProp, MM::ActionType eAct);
    int OnHeight(MM::PropertyBase* pProp, MM::ActionType eAct);
    int OnTemperature(MM::PropertyBase* pProp, MM::ActionType eAct);
    int OnPixelType(MM::PropertyBase* pProp, MM::ActionType eAct);
    int OnGain(MM::PropertyBase* pProp, MM::ActionType eAct);
    int OnExposure(MM::PropertyBase* pProp, MM::ActionType eAct);

private:
    friend class SequenceThread;
    static const int MAX_BIT_DEPTH = 12;

    ImgBuffer img_;
    SequenceThread* thd_;
    int bytesPerPixel_;
    int bitsPerPixel_;
    bool initialized_;
    bool sequenceRunning_;

    int ResizeImageBuffer();
    void GenerateImage();
    int InsertImage();

    std::vector<std::string> acqDeviceList_;
    std::string activeDevice_;

    SapAcqDevice AcqDevice_;
    SapBufferWithTrash Buffers_;
    SapBufferRoi Roi_;
    SapTransfer AcqToBuf_;
    SapTransfer AcqDeviceToBuf_;
    SapTransfer* Xfer_;
    SapLocation loc_;
    SapFeature feature_;
    //int SapFormatBytes_;

    int FreeHandles();
    int ErrorBox(std::string text, std::string caption);
    int SetUpBinningProperties();
    LPCWSTR SaperaGigE::string2winstring(const std::string& s);
    int SapBufferReformat(std::string pixelFormat="");
};

//threading stuff.  Tread lightly
class SequenceThread : public MMDeviceThreadBase
{
public:
    SequenceThread(SaperaGigE* pCam) : stop_(false), numImages_(0) { camera_ = pCam; }
    ~SequenceThread() {}

    int svc(void);

    void Stop() { stop_ = true; }

    void Start()
    {
        stop_ = false;
        activate();
    }

    void SetLength(long images) { numImages_ = images; }
    long GetLength(void) { return numImages_; };

private:
    SaperaGigE* camera_;
    bool stop_;
    long numImages_;
};

#endif //_SaperaGigE_H_
