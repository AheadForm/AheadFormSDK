#include "Media.h"

#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <strings.h>

int Media_LoadMp3Pcm(const char *filePath, unsigned char **ppPcm, unsigned int *pPcmSize)
{
	GstElement *pPipeline = NULL, *pSource = NULL, *pSink = NULL;
	GstBus *pBus = NULL;
	GstSample *pSample = NULL;
	GstBuffer *pBuffer = NULL;
	GstMessage *pMessage = NULL;
	GstMapInfo stMap;
	GError *pError = NULL;
	gchar *strDebug = NULL;
	unsigned char *pPcm = NULL, *pNewPcm = NULL;
	size_t size = 0, capacity = 0, needed;
	gint64 lastDataTime;
	int ret = RET_SUCCESS;

	if (filePath == NULL || strlen(filePath) < 5 || strcasecmp(filePath + strlen(filePath) - 4, ".mp3") != 0)
		return ERR_ARGV_1;

	if (ppPcm == NULL || pPcmSize == NULL)
		return ERR_ARGV;

	*ppPcm = NULL;
	*pPcmSize = 0;
	if (!gst_init_check(NULL, NULL, &pError))
	{
		printf("%s() gst_init_check() failed: %s\r\n", __FUNCTION__, pError != NULL ? pError->message : "unknown error");
		g_clear_error(&pError);
		return ERR_INIT_FAIL;
	}
	pPipeline = gst_parse_launch("filesrc name=input ! mpegaudioparse ! audio/mpeg,mpegversion=1,layer=3 ! mpg123audiodec ! audioconvert ! audioresample "
		"! audio/x-raw,format=S16LE,rate=16000,channels=1,layout=interleaved ! appsink name=output sync=false max-buffers=8", &pError);
	if (pError != NULL || pPipeline == NULL)
	{
		printf("%s() gst_parse_launch(MP3 decoder) failed: %s\r\n", __FUNCTION__, pError != NULL ? pError->message : "missing pipeline");
		g_clear_error(&pError);
		if (pPipeline != NULL)
			gst_object_unref(pPipeline);

		return ERR_INIT_FAIL;
	}
	pSource = gst_bin_get_by_name(GST_BIN(pPipeline), "input");
	pSink = gst_bin_get_by_name(GST_BIN(pPipeline), "output");
	pBus = gst_element_get_bus(pPipeline);
	g_object_set(pSource, "location", filePath, NULL);
	if (gst_element_set_state(pPipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE)
		ret = ERR_FILE_FORMAT;

	lastDataTime = g_get_monotonic_time();
	while (ret == RET_SUCCESS)
	{
		pSample = gst_app_sink_try_pull_sample(GST_APP_SINK(pSink), 100 * GST_MSECOND);
		if (pSample != NULL)
		{
			pBuffer = gst_sample_get_buffer(pSample);
			if (pBuffer == NULL || !gst_buffer_map(pBuffer, &stMap, GST_MAP_READ))
			{
				gst_sample_unref(pSample);
				ret = ERR_FILE_FORMAT;
				break;
			}
			needed = size + stMap.size;
			if (stMap.size > UINT_MAX - size)
				ret = ERR_OVER_BUF;

			if (ret == RET_SUCCESS && needed > capacity)
			{
				capacity = needed > UINT_MAX - 1048576 ? needed : needed + 1048576;
				pNewPcm = realloc(pPcm, capacity);
				if (pNewPcm == NULL)
					ret = ERR_MEMORY_ALLOC;
				else
					pPcm = pNewPcm;
			}

			if (ret == RET_SUCCESS)
			{
				memcpy(pPcm + size, stMap.data, stMap.size);
				size = needed;
				lastDataTime = g_get_monotonic_time();
			}
			gst_buffer_unmap(pBuffer, &stMap);
			gst_sample_unref(pSample);
		}
		pMessage = gst_bus_pop_filtered(pBus, GST_MESSAGE_ERROR);
		if (pMessage != NULL)
		{
			gst_message_parse_error(pMessage, &pError, &strDebug);
			printf("%s() decode MP3(%s) failed: %s\r\n", __FUNCTION__, filePath, pError->message);
			g_clear_error(&pError);
			g_free(strDebug);
			gst_message_unref(pMessage);
			ret = ERR_FILE_FORMAT;
		}

		if (pSample == NULL && gst_app_sink_is_eos(GST_APP_SINK(pSink)))
			break;

		if (g_get_monotonic_time() - lastDataTime > 5000000)
		{
			printf("%s() decode MP3(%s) stalled for 5 seconds\r\n", __FUNCTION__, filePath);
			ret = ERR_REQUEST_TIMEOUT;
		}
	}
	gst_element_set_state(pPipeline, GST_STATE_NULL);
	gst_object_unref(pBus);
	gst_object_unref(pSource);
	gst_object_unref(pSink);
	gst_object_unref(pPipeline);
	if (ret == RET_SUCCESS && (size == 0 || size % 2 != 0))
		ret = ERR_FILE_FORMAT;

	if (ret != RET_SUCCESS)
	{
		free(pPcm);
		return ret;
	}
	*ppPcm = pPcm;
	*pPcmSize = (unsigned int)size;
	return RET_SUCCESS;
}
