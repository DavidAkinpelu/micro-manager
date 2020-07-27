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
#include "../MMDevice/ModuleInterface.h"
#include <string>
#include <functional>

//////////////////////////////////////////////////////////////////////////////
// Error codes
//
#define ERR_UNKNOWN_MODE         102

class SequenceThread;

const char* g_CameraName = "SaperaGigE";
const char* g_CameraServer = "AcquisitionDevice";
const char* g_ShutterMode = "ShutterMode";
const char* g_BinningMode = "binningMode";

std::map< SapFeature::Type, MM::PropertyType > featureType = {
    {SapFeature::TypeString, MM::String},
    {SapFeature::TypeEnum, MM::String},
    {SapFeature::TypeInt32, MM::Integer},
    {SapFeature::TypeFloat, MM::Float},
    {SapFeature::TypeDouble, MM::Float},
    {SapFeature::TypeUndefined, MM::String}
};

class SaperaGigE : public CCameraBase<SaperaGigE>
{
private:
    struct myFeature
    {
        char* name;
        bool readOnly;
        CPropertyAction* action;
    };

    const std::map< const char*, myFeature > deviceFeatures = {
        // information on device - use names shown in Sapera CamExpert
        {MM::g_Keyword_PixelType, {"PixelFormat", false, new CPropertyAction(this, &SaperaGigE::OnPixelType)}},
        {"Manufacturer Name", {"DeviceVendorName", true, NULL}},
        {"Family Name", {"DeviceFamilyName", true, NULL}},
        {"Model Name", {"DeviceModelName", true, NULL}},
        {"Device Version", {"DeviceVersion", true, NULL}},
        {"Manufacturer Info", {"DeviceManufacturerInfo", true, NULL}},
        {"Manufacturer Part Number", {"deviceManufacturerPartNumber", true, NULL}},
        {"Firmware Version", {"DeviceFirmwareVersion", true, NULL}},
        {"Serial Number", {"DeviceSerialNumber", true, NULL}},
        {"Device User ID", {"DeviceUserID", true, NULL}},
        {"MAC Address", {"deviceMacAddress", true, NULL}},
        {"SensorType", {"sensorColorType", true, NULL}},
        {"SensorPixelCoding", {"PixelCoding", true, NULL}},
        {"SensorBlackLevel", {"BlackLevel", true, NULL}},
        {"SensorPixelInput", {"pixelSizeInput", true, NULL}},
        {"SensorShutterMode", {"SensorShutterMode", false, NULL}},
        {"SensorBinningMode", {"binningMode", false, new CPropertyAction(this, &SaperaGigE::OnBinningMode)}},
        {"SensorWidth", {"SensorWidth", true, NULL}},
        {"SensorHeight", {"SensorHeight", true, NULL}},
        {"ImagePixelSize", {"PixelSize", true, new CPropertyAction(this, &SaperaGigE::OnPixelSize)}},
        {"ImageHorizontalOffset", {"OffsetX", false, new CPropertyAction(this, &SaperaGigE::OnOffsetX)}},
        {"ImageVerticalOffset", {"OffsetY", false, new CPropertyAction(this, &SaperaGigE::OnOffsetY)}},
        {"ImageWidth", {"Width", false, new CPropertyAction(this, &SaperaGigE::OnWidth)}},
        {"ImageHeight", {"Height", false, new CPropertyAction(this, &SaperaGigE::OnHeight)}},
        {"DeviceTemperature", {"DeviceTemperature", true, new CPropertyAction(this, &SaperaGigE::OnTemperature)}},
    };

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
    int OnBinningMode(MM::PropertyBase* pProp, MM::ActionType eAct);
    int OnPixelSize(MM::PropertyBase* pProp, MM::ActionType eAct);
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
    SapBufferRoi* Roi_;
    SapTransfer AcqToBuf_;
    SapTransfer AcqDeviceToBuf_;
    SapTransfer* Xfer_;
    SapLocation loc_;
    SapFeature AcqFeature_;

    int FreeHandles();
    int ErrorBox(std::string text, std::string caption);
    int SetUpBinningProperties();
    LPCWSTR SaperaGigE::string2winstring(const std::string& s);
    int SynchronizeBuffers(std::string pixelFormat = "", int width = -1, int height = -1);
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
