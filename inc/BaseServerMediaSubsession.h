/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** BaseServerMediaSubsession.h
** 
** -------------------------------------------------------------------------*/

#pragma once

#include <sys/stat.h>

#include <string>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <sstream>

// live555
#include <liveMedia.hh>

// libv4l2cpp
#include "V4l2Device.h"

// v4l2rtspserver
#include "V4L2DeviceSource.h"
#include "logger.h"

#ifdef HAVE_ALSA   
#include "ALSACapture.h"
#endif

// ---------------------------------
//   BaseServerMediaSubsession
// ---------------------------------
class BaseServerMediaSubsession
{
	public:
		BaseServerMediaSubsession(StreamReplicator* replicator): m_replicator(replicator) {
			V4L2DeviceSource* deviceSource = dynamic_cast<V4L2DeviceSource*>(replicator->inputSource());
			if (deviceSource) {
                DeviceInterface* device = deviceSource->getDevice();
                if (device->getVideoFormat() >= 0) {
    	            m_format = BaseServerMediaSubsession::getVideoRtpFormat(device->getVideoFormat());		
                } else {
    	            m_format = BaseServerMediaSubsession::getAudioRtpFormat(device->getAudioFormat(), device->getSampleRate(), device->getChannels());						
                }
                LOG(NOTICE) << "RTP format:" << m_format;
			}
	    }

		// -----------------------------------------
        //    convert V4L2 pix format to RTP mime
        // -----------------------------------------
        static std::string getVideoRtpFormat(int format)
        {
            std::string rtpFormat;
            switch(format)
            {	
                case V4L2_PIX_FMT_HEVC  : rtpFormat = "video/H265"; break;
                case V4L2_PIX_FMT_H264  : rtpFormat = "video/H264"; break;
                case V4L2_PIX_FMT_MJPEG : rtpFormat = "video/JPEG"; break;
                case V4L2_PIX_FMT_JPEG  : rtpFormat = "video/JPEG"; break;
                case V4L2_PIX_FMT_VP8   : rtpFormat = "video/VP8" ; break;
                case V4L2_PIX_FMT_VP9   : rtpFormat = "video/VP9" ; break;
                case V4L2_PIX_FMT_YUV444: rtpFormat = "video/RAW" ; break;
                case V4L2_PIX_FMT_UYVY  : rtpFormat = "video/RAW" ; break;
                case V4L2_PIX_FMT_NV12  : rtpFormat = "video/RAW" ; break;
                case V4L2_PIX_FMT_BGR24 : rtpFormat = "video/RAW" ; break;
                case V4L2_PIX_FMT_BGR32 : rtpFormat = "video/RAW" ; break;
                case V4L2_PIX_FMT_RGB24 : rtpFormat = "video/RAW" ; break;
                case V4L2_PIX_FMT_RGB32 : rtpFormat = "video/RAW" ; break;
            }
            
            return rtpFormat;
        }

        static std::string getAudioRtpFormat(std::string format, int sampleRate, int channels)
        {
            std::ostringstream os;
#ifdef HAVE_ALSA            
            os << "audio/" << format << "/" << sampleRate << "/" << channels;
#endif            
            return os.str();
        }        
	
	public:
		static FramedSource* createSource(UsageEnvironment& env, FramedSource * videoES, const std::string& format);
		static RTPSink* createSink(UsageEnvironment& env, Groupsock * rtpGroupsock, unsigned char rtpPayloadTypeIfDynamic, const std::string& format, V4L2DeviceSource* source);
		char const* getAuxLine(V4L2DeviceSource* source, RTPSink* rtpSink);
		
	protected:
		StreamReplicator* m_replicator;
		std::string m_format;        
};

