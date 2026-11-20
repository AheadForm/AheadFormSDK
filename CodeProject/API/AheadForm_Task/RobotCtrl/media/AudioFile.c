#include "AudioFile.h"

#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <string.h>

int Audio_DecodeFileToPcm(const char *filePath, void *pRetPcmBuf, size_t pcmBufSize, AHEADFORM_AUDIO_FILE_INFO *pstRetInfo)
{
	GstElement *pPipeline = NULL, *pSource = NULL, *pSink = NULL;
	GstBus *pBus = NULL;
	GstSample *pSample = NULL;
	GstBuffer *pBuffer = NULL;
	GstMessage *pMessage = NULL;
	GstMapInfo stMap;
	GError *pError = NULL;
	size_t dataLen = 0;
	int ret = RET_SUCCESS;

	if (filePath == NULL || filePath[0] == 0)
		return ERR_ARGV_1;

	if (pRetPcmBuf == NULL)
		return ERR_ARGV_2;

	if (pcmBufSize < 2)
		return ERR_ARGV_3;

	if (pstRetInfo == NULL)
		return ERR_ARGV_4;

	if (!gst_init_check(NULL, NULL, &pError))
	{
		printf("%s() call gst_init_check() failed: %s\r\n", __FUNCTION__, pError != NULL ? pError->message : "no GStreamer error detail");
		g_clear_error(&pError);
		return ERR_INIT_FAIL;
	}

	if ( (pPipeline = gst_parse_launch("filesrc name=source ! decodebin ! audioconvert ! audioresample ! audio/x-raw,format=S16LE,rate=16000,channels=1,layout=interleaved ! appsink name=pcm sync=false max-buffers=8 wait-on-eos=false", &pError)) == NULL || pError != NULL )
	{
		printf("%s() call gst_parse_launch(audio decoder) failed: %s\r\n", __FUNCTION__, pError != NULL ? pError->message : "no decoder pipeline");
		g_clear_error(&pError);
		if (pPipeline != NULL)
			gst_object_unref(pPipeline);

		return ERR_INIT_FAIL;
	}
	pSource = gst_bin_get_by_name(GST_BIN(pPipeline), "source");
	pSink = gst_bin_get_by_name(GST_BIN(pPipeline), "pcm");
	pBus = gst_element_get_bus(pPipeline);
	if (pSource == NULL || pSink == NULL || pBus == NULL)
	{
		printf("%s() cannot get audio decoder source, sink or bus.\r\n", __FUNCTION__);
		ret = ERR_INIT_FAIL;
	}
	else
	{
		g_object_set(pSource, "location", filePath, NULL);
		if (gst_element_set_state(pPipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE)
		{
			printf("%s() call gst_element_set_state(file = %s, PLAYING) failed.\r\n", __FUNCTION__, filePath);
			ret = ERR_INIT_FAIL;
		}

		while (ret == RET_SUCCESS)
		{
			pSample = gst_app_sink_try_pull_sample(GST_APP_SINK(pSink), 100 * GST_MSECOND);
			if (pSample != NULL)
			{
				pBuffer = gst_sample_get_buffer(pSample);
				if (pBuffer == NULL)
				{
					ret = ERR_DATA_INCOMPLETE;
				}
				else if (!gst_buffer_map(pBuffer, &stMap, GST_MAP_READ))
				{
					printf("%s() call gst_buffer_map(audio file = %s) failed.\r\n", __FUNCTION__, filePath);
					ret = ERR_FILE_FORMAT;
				}
				else
				{
					if (stMap.size % 2 != 0)
					{
						ret = ERR_DATA_INCOMPLETE;
					}
					else if (stMap.size > pcmBufSize - dataLen)
					{
						ret = ERR_OVER_BUF;
					}
					else if (stMap.size > 0)
					{
						memcpy((unsigned char*)pRetPcmBuf + dataLen, stMap.data, stMap.size);
						dataLen += stMap.size;
					}
					gst_buffer_unmap(pBuffer, &stMap);
				}
				gst_sample_unref(pSample);
			}
			else
			{
				if ( (pMessage = gst_bus_pop_filtered(pBus, GST_MESSAGE_ERROR | GST_MESSAGE_EOS)) != NULL )
				{
					if (GST_MESSAGE_TYPE(pMessage) == GST_MESSAGE_EOS)
					{
						gst_message_unref(pMessage);
						pMessage = NULL;
						break;
					}
					ret = ERR_FILE_FORMAT;
				}
			}
		}

		if (pMessage == NULL)
			pMessage = gst_bus_pop_filtered(pBus, GST_MESSAGE_ERROR);

		if (pMessage != NULL)
		{
			gst_message_parse_error(pMessage, &pError, NULL);
			printf("%s() Audio decode failed for %s: %s\r\n", __FUNCTION__, filePath, pError != NULL ? pError->message : "no decoder error detail");
			g_clear_error(&pError);
			gst_message_unref(pMessage);
			if (ret == RET_SUCCESS)
				ret = ERR_FILE_FORMAT;
		}
	}

	if (gst_element_set_state(pPipeline, GST_STATE_NULL) == GST_STATE_CHANGE_FAILURE)
	{
		printf("%s() call gst_element_set_state(file = %s, NULL) failed.\r\n", __FUNCTION__, filePath);
		if (ret == RET_SUCCESS)
			ret = ERR_INIT_FAIL;
	}

	if (pBus != NULL)
		gst_object_unref(pBus);

	if (pSink != NULL)
		gst_object_unref(pSink);

	if (pSource != NULL)
		gst_object_unref(pSource);

	gst_object_unref(pPipeline);
	if (ret != RET_SUCCESS)
		return ret;

	if (dataLen == 0)
		return ERR_FILE_FORMAT;

	pstRetInfo->stFormat.SampleRate_Hz = 16000;
	pstRetInfo->stFormat.channelCnt = 1;
	pstRetInfo->sampleFrameCnt = dataLen / 2;
	pstRetInfo->duration_us = pstRetInfo->sampleFrameCnt / 16000 * 1000000 + pstRetInfo->sampleFrameCnt % 16000 * 1000000 / 16000;
	return RET_SUCCESS;
}
