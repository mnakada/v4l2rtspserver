/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** ALSACapture.cpp
** 
** V4L2 RTSP streamer                                                                 
**                                                                                    
** ALSA capture overide of V4l2Capture
**                                                                                    
** -------------------------------------------------------------------------*/

#ifdef HAVE_ALSA

#include <sys/time.h>

#include "ALSACapture.h"

static const snd_pcm_format_t formats[] = {
	SND_PCM_FORMAT_S8,
	SND_PCM_FORMAT_U8,
	SND_PCM_FORMAT_S16_LE,
	SND_PCM_FORMAT_S16_BE,
	SND_PCM_FORMAT_U16_LE,
	SND_PCM_FORMAT_U16_BE,
	SND_PCM_FORMAT_S24_LE,
	SND_PCM_FORMAT_S24_BE,
	SND_PCM_FORMAT_U24_LE,
	SND_PCM_FORMAT_U24_BE,
	SND_PCM_FORMAT_S32_LE,
	SND_PCM_FORMAT_S32_BE,
	SND_PCM_FORMAT_U32_LE,
	SND_PCM_FORMAT_U32_BE,
	SND_PCM_FORMAT_FLOAT_LE,
	SND_PCM_FORMAT_FLOAT_BE,
	SND_PCM_FORMAT_FLOAT64_LE,
	SND_PCM_FORMAT_FLOAT64_BE,
	SND_PCM_FORMAT_IEC958_SUBFRAME_LE,
	SND_PCM_FORMAT_IEC958_SUBFRAME_BE,
	SND_PCM_FORMAT_MU_LAW,
	SND_PCM_FORMAT_A_LAW,
	SND_PCM_FORMAT_IMA_ADPCM,
	SND_PCM_FORMAT_MPEG,
	SND_PCM_FORMAT_GSM,
	SND_PCM_FORMAT_SPECIAL,
	SND_PCM_FORMAT_S24_3LE,
	SND_PCM_FORMAT_S24_3BE,
	SND_PCM_FORMAT_U24_3LE,
	SND_PCM_FORMAT_U24_3BE,
	SND_PCM_FORMAT_S20_3LE,
	SND_PCM_FORMAT_S20_3BE,
	SND_PCM_FORMAT_U20_3LE,
	SND_PCM_FORMAT_U20_3BE,
	SND_PCM_FORMAT_S18_3LE,
	SND_PCM_FORMAT_S18_3BE,
	SND_PCM_FORMAT_U18_3LE,
	SND_PCM_FORMAT_U18_3BE,
};

ALSACapture* ALSACapture::createNew(const ALSACaptureParameters & params) 
{ 
	ALSACapture* capture = new ALSACapture(params);
	if (capture) 
	{
		if (capture->getFd() == -1) 
		{
			delete capture;
			capture = NULL;
		}
	}
	return capture; 
}

ALSACapture::~ALSACapture()
{
	if(m_opus) opus_encoder_destroy(m_opus);
	if(m_pcm_buffer)
	{
		delete[] m_pcm_buffer;
		m_pcm_buffer = NULL;
	}
	this->close();
}

void ALSACapture::close()
{
	if (m_pcm != NULL)
	{
		snd_pcm_close (m_pcm);
		m_pcm = NULL;
	}
}
	
ALSACapture::ALSACapture(const ALSACaptureParameters & params) : m_pcm(NULL), m_bufferSize(0), m_periodSize(0), m_params(params), m_pcm_buffer(NULL)
{
	LOG(NOTICE) << "Open ALSA device: \"" << params.m_devName << "\"";
	
	snd_pcm_hw_params_t *hw_params = NULL;
	int err = 0;
	
	// open PCM device
	if ((err = snd_pcm_open (&m_pcm, m_params.m_devName.c_str(), SND_PCM_STREAM_CAPTURE, 0)) < 0) {
		LOG(ERROR) << "cannot open audio device: " << m_params.m_devName << " error:" <<  snd_strerror (err);
	}
				
	// configure hw_params
	else if ((err = snd_pcm_hw_params_malloc (&hw_params)) < 0) {
		LOG(ERROR) << "cannot allocate hardware parameter structure device: " << m_params.m_devName << " error:" <<  snd_strerror (err);
		this->close();
	}
	else if ((err = snd_pcm_hw_params_any (m_pcm, hw_params)) < 0) {
		LOG(ERROR) << "cannot initialize hardware parameter structure device: " << m_params.m_devName << " error:" <<  snd_strerror (err);
		this->close();
	}			
	else if ((err = snd_pcm_hw_params_set_access (m_pcm, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
		LOG(ERROR) << "cannot set access type device: " << m_params.m_devName << " error:" <<  snd_strerror (err);
		this->close();
	}
	else if (this->configureFormat(hw_params) < 0) {
		this->close();
	}
	else if ((err = snd_pcm_hw_params_set_rate_near (m_pcm, hw_params, &m_params.m_sampleRate, 0)) < 0) {
		LOG(ERROR) << "cannot set sample rate device: " << m_params.m_devName << " error:" <<  snd_strerror (err);
		this->close();
	}
	else if ((err = snd_pcm_hw_params_set_channels (m_pcm, hw_params, m_params.m_channels)) < 0) {
		LOG(ERROR) << "cannot set channel count device: " << m_params.m_devName << " error:" <<  snd_strerror (err);
		this->close();
	}
	
	m_opus = NULL;
	if(!err) {
		if (m_params.m_format == std::string("OPUS")) {
			// opus initialize
			m_bufferSize = 4096;
			m_periodSize = m_params.m_sampleRate * 20 / 1000;
			m_pcm_buffer = new char[m_periodSize * snd_pcm_format_physical_width(m_fmt) / 8];
			m_opus = opus_encoder_create(m_params.m_sampleRate, m_params.m_channels, OPUS_APPLICATION_VOIP, &err);
			if((m_opus == NULL) || (err != 0)) {
				LOG(ERROR) << "opus_encoder_create : " << opus_strerror (err);
				this->close();
			} else {
				opus_encoder_ctl(m_opus, OPUS_SET_LSB_DEPTH(snd_pcm_format_physical_width(m_fmt)));
			}
		} else {
			// PCM initialize
			m_periodSize = m_params.m_sampleRate * 120 / 1000;
			m_bufferSize = m_periodSize * snd_pcm_format_physical_width(m_fmt) / 8 * m_params.m_channels;
		}
	}

	if(!err) {
		if ((err = snd_pcm_hw_params_set_period_size(m_pcm, hw_params, m_periodSize, 0)) < 0) {
			LOG(ERROR) << "cannot set sample rate device: " << m_params.m_devName << " periodSize: " << m_periodSize << " error:" <<  snd_strerror (err);
			this->close();
		}
		else if ((err = snd_pcm_hw_params (m_pcm, hw_params)) < 0) {
			LOG(ERROR) << "cannot set parameters device: " << m_params.m_devName << " error:" <<  snd_strerror (err);
			this->close();
		}
		// start capture
		else if ((err = snd_pcm_prepare (m_pcm)) < 0) {
			LOG(ERROR) << "cannot prepare audio interface for use device: " << m_params.m_devName << " error:" <<  snd_strerror (err);
			this->close();
		}
		else if ((err = snd_pcm_start (m_pcm)) < 0) {
			LOG(ERROR) << "cannot start audio interface for use device: " << m_params.m_devName << " error:" <<  snd_strerror (err);
			this->close();
		}
	}

	if (!err) {
		// get supported format
		for (int i = 0; i < (int)(sizeof(formats)/sizeof(formats[0])); ++i) {
			if (!snd_pcm_hw_params_test_format(m_pcm, hw_params, formats[i])) {
				m_fmtList.push_back(formats[i]);
			}
		}	
	}

	LOG(NOTICE) << "ALSA device: \"" << m_params.m_devName << "\" buffer_size:" << m_bufferSize << " period_size:" << m_periodSize << " rate:" << m_params.m_sampleRate;
}
			
int ALSACapture::configureFormat(snd_pcm_hw_params_t *hw_params) {
	
	// try to set format, widht, height
	std::list<snd_pcm_format_t>::iterator it;
	for (it = m_params.m_formatList.begin(); it != m_params.m_formatList.end(); ++it) {
		snd_pcm_format_t format = *it;
		int err = snd_pcm_hw_params_set_format (m_pcm, hw_params, format);
		if (err < 0) {
			LOG(NOTICE) << "cannot set sample format device: " << m_params.m_devName << " to:" << format << " error:" <<  snd_strerror (err);
		} else {
			LOG(NOTICE) << "set sample format device: " << m_params.m_devName << " to:" << format << " ok";
			m_fmt = format;
			return 0;
		}		
	}
	return -1;
}

size_t ALSACapture::read(char* buffer, size_t bufferSize)
{
	static timeval lastTime;
	size_t size = 0;
	int fmt_phys_width_bytes = 0;
	if (m_pcm != 0)
	{
		fmt_phys_width_bytes = snd_pcm_format_physical_width(m_fmt) / 8;
		timeval curTime;
		gettimeofday(&curTime, NULL);
		timeval diff;
		timersub(&curTime, &lastTime, &diff);
		lastTime = curTime;

		if(m_params.m_format == "OPUS") {
			snd_pcm_sframes_t frameSize = snd_pcm_readi(m_pcm, m_pcm_buffer, m_periodSize);
			LOG(DEBUG) << "pcm_readi periodSize:" << m_periodSize * fmt_phys_width_bytes << " frameSize:" << frameSize * fmt_phys_width_bytes;
			if (frameSize > 0) {
				size = opus_encode(m_opus, (opus_int16 *)m_pcm_buffer, frameSize, (unsigned char *)buffer, bufferSize);
				LOG(DEBUG) << "opus_encode pcm frameSize: " << frameSize * fmt_phys_width_bytes << "bytes opus outSize: " << size << "bytes interval: " <<  (diff.tv_sec*1000+diff.tv_usec/1000) << "ms\n";
			}
		} else {
			snd_pcm_sframes_t frameSize = snd_pcm_readi(m_pcm, buffer, m_periodSize);
			LOG(DEBUG) << "pcm_readi periodSize:" << m_periodSize * fmt_phys_width_bytes << " frameSize:" << frameSize * fmt_phys_width_bytes;
			if (frameSize > 0) {
				// swap if capture in not in network order
				if (!snd_pcm_format_big_endian(m_fmt)) {
					for(unsigned int i = 0; i < frameSize * m_params.m_channels; i++) {
						unsigned short *ptr = (unsigned short *)buffer + i;
						*ptr = (*ptr >> 8) | (*ptr << 8);
					}
				}
				size = frameSize * m_params.m_channels * fmt_phys_width_bytes;
				LOG(DEBUG) << "pcm frameSize: " << size << "bytes interval: " <<  (diff.tv_sec*1000+diff.tv_usec/1000) << "ms";
			}
		}
	}
	return size;
}
		
int ALSACapture::getFd()
{
	unsigned int nbfs = 1;
	struct pollfd pfds[nbfs]; 
	pfds[0].fd = -1;
	
	if (m_pcm != 0)
	{
		int count = snd_pcm_poll_descriptors_count (m_pcm);
		int err = snd_pcm_poll_descriptors(m_pcm, pfds, count);
		if (err < 0) {
			fprintf (stderr, "cannot snd_pcm_poll_descriptors (%s)\n", snd_strerror (err));
		}
	}
	return pfds[0].fd;
}
		
#endif


