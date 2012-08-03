
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "bcm_host.h"
#include "ilclient.h"

static int image_decode_test(char *filename)
{

     OMX_BUFFERHEADERTYPE *pIbuf;
     int bytes_read;
     int status = 0, ret;
     int port_settings_changed = 0;

     OMX_IMAGE_PARAM_PORTFORMATTYPE imagePortFormat;
     COMPONENT_T *image_decode = NULL;
     COMPONENT_T *video_render = NULL;
     COMPONENT_T *list[3];
     TUNNEL_T tunnel[2];
     ILCLIENT_T *client;
     FILE *fin;

     memset(list, 0, sizeof(list));
     memset(tunnel, 0, sizeof(tunnel));

     if((fin = fopen(filename, "rb")) == NULL ) {
	  perror(filename);
	  return -1;
     }

     if((client = ilclient_init()) == NULL) {
	  return -3;
     }     

     if(OMX_Init() != OMX_ErrorNone) {
	  ilclient_destroy(client);
	  fclose(fin);
	  return -4;
     }

#if 0 //this was an attempt to use the image_read component
     COMPONENT_T *image_read;
     // create image_read
     if(ilclient_create_component(client, &image_read, "image_read",
				  ILCLIENT_DISABLE_ALL_PORTS ) != 0)
	  status = -14;


     //set URI
     OMX_PARAM_CONTENTURITYPE *pUri;
     sz = sizeof(pUri)+strlen(filename);
     pUri = (OMX_PARAM_CONTENTURITYPE*)calloc(1, sz);
     pUri->nSize = sz;
     pUri->nVersion.nVersion = OMX_VERSION;
     strcpy(pUri->contentURI, filename);
     if(image_read != NULL && OMX_SetParameter(ILC_GET_HANDLE(image_read), OMX_IndexParamContentURI, pUri) != 0)
	  status = -17;

     //enable port
     ilclient_enable_port(image_read, 310);

     //get OMX_IndexParamPortDefinition
     {
	  OMX_PARAM_PORTDEFINITIONTYPE portDef;
	  portDef.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
	  portDef.nVersion.nVersion = OMX_VERSION;
	  portDef.nPortIndex = 310;
	  ret = OMX_GetParameter(ILC_GET_HANDLE(image_read), OMX_IndexParamPortDefinition, &portDef);
     }

     //get OMX_IndexParamImagePortFormat
     i=0;
     while(ret == 0) {
	  OMX_IMAGE_PARAM_PORTFORMATTYPE portFormat;
	  portFormat.nSize = sizeof(OMX_IMAGE_PARAM_PORTFORMATTYPE);
	  portFormat.nVersion.nVersion = OMX_VERSION;
	  portFormat.nPortIndex = 310;
	  portFormat.nIndex = i;
	  ret = OMX_GetParameter(ILC_GET_HANDLE(image_read), OMX_IndexParamImagePortFormat, &portFormat);
	  i++;
     }

     {
	  OMX_IMAGE_PARAM_PORTFORMATTYPE portFormat;
	  portFormat.nSize = sizeof(OMX_IMAGE_PARAM_PORTFORMATTYPE);
	  portFormat.nVersion.nVersion = OMX_VERSION;
	  portFormat.nPortIndex = 310;
	  portFormat.eCompressionFormat = OMX_IMAGE_CodingJPEG;

	  ret = OMX_SetParameter(ILC_GET_HANDLE(image_read), OMX_IndexParamImagePortFormat, &portFormat);
     }

     if((ret = ilclient_change_component_state(image_read, OMX_StateIdle) < 0)) //this fails
	  printf("Cannot change image_read component state to OMX_StateIdle\n"); 

#endif //image_read attempt


     // create image_decode
     ret = ilclient_create_component(client, &image_decode, "image_decode", 
				     ILCLIENT_DISABLE_ALL_PORTS | 
				     ILCLIENT_ENABLE_INPUT_BUFFERS );
     if (ret != 0)
	  status = -15;
     list[0] = image_decode;

     // create video_render
     ret = ilclient_create_component(client, &video_render, "video_render", 
				   ILCLIENT_DISABLE_ALL_PORTS);
     if (ret != 0)
	  status = -14;
     list[1] = video_render;
     list[2] = NULL;
     

     ret = ilclient_change_component_state(image_decode, OMX_StateIdle);

     set_tunnel(tunnel, image_decode, 321, video_render, 90);

#if 0 //not necessary
     {
	  //get OMX_IndexParamPortDefinition
	  OMX_PARAM_PORTDEFINITIONTYPE portDef;
	  portDef.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
	  portDef.nVersion.nVersion = OMX_VERSION;
	  portDef.nPortIndex = 320;
	  ret = OMX_GetParameter(ILC_GET_HANDLE(image_decode), OMX_IndexParamPortDefinition, &portDef);
     }
#endif

     {
	  // OMX_IndexParamImagePortFormat
	  imagePortFormat.nSize = sizeof(OMX_IMAGE_PARAM_PORTFORMATTYPE);
	  imagePortFormat.nVersion.nVersion = OMX_VERSION;
	  imagePortFormat.nPortIndex = 320;
	  imagePortFormat.eCompressionFormat = OMX_IMAGE_CodingJPEG;
//	  imagePortFormat.eCompressionFormat = OMX_IMAGE_CodingPNG; //does not seem to work
//	  imagePortFormat.eCompressionFormat = OMX_IMAGE_CodingAutoDetect; //does not seem to work.

	  ret = OMX_SetParameter(ILC_GET_HANDLE(image_decode), OMX_IndexParamImagePortFormat, &imagePortFormat);
     }

     ret = ilclient_enable_port_buffers(image_decode, 320, NULL, NULL, NULL);

     ret = ilclient_change_component_state(image_decode, OMX_StateExecuting);

     while( (pIbuf = ilclient_get_input_buffer(image_decode, 320, 1)) != NULL ) {
	  bytes_read = fread(pIbuf->pBuffer, 1, pIbuf->nAllocLen, fin);

	  if( port_settings_changed == 0 &&
	      (( bytes_read > 0 && ilclient_remove_event(image_decode, OMX_EventPortSettingsChanged, 321, 0, 0, 1) == 0) ||
	       ( bytes_read == 0 && ilclient_wait_for_event(image_decode, OMX_EventPortSettingsChanged, 321, 0, 0, 1,
							    ILCLIENT_EVENT_ERROR | ILCLIENT_PARAMETER_CHANGED, 10000) == 0)))
	  {
	       port_settings_changed = 1;

		    
	       if(ilclient_setup_tunnel(tunnel, 0, 0) != 0)
	       {
		    status = -7;
		    break;
	       }
		    
	       ilclient_change_component_state(video_render, OMX_StateExecuting);

	  }

	  if(!bytes_read)
	       break;
	       
	  pIbuf->nFilledLen = bytes_read;
	  pIbuf->nOffset = 0;
	  pIbuf->nFlags = OMX_BUFFERFLAG_TIME_UNKNOWN;

	  if(OMX_EmptyThisBuffer(ILC_GET_HANDLE(image_decode), pIbuf) != OMX_ErrorNone)
	  {
	       status = -6;
	       break;
	  }
     }

     pIbuf->nFilledLen = 0;
     pIbuf->nFlags = OMX_BUFFERFLAG_TIME_UNKNOWN | OMX_BUFFERFLAG_EOS;
      
     if(OMX_EmptyThisBuffer(ILC_GET_HANDLE(image_decode), pIbuf) != OMX_ErrorNone)
	  status = -20;
      

     // wait for EOS from render
     ilclient_wait_for_event(video_render, OMX_EventBufferFlag, 90, 0, OMX_BUFFERFLAG_EOS, 0,
			     ILCLIENT_BUFFER_FLAG_EOS, 10000);
      

     printf("Press ENTER to continue...\n");
     fflush(stdout);
     getchar();

     
     // need to flush the renderer to allow video_decode to disable its input port
//     ilclient_flush_tunnels(tunnel, 0);

     ilclient_disable_port_buffers(image_decode, 320, NULL, NULL, NULL);

     fclose(fin);

     ilclient_disable_tunnel(tunnel);
     ilclient_teardown_tunnels(tunnel);

      
     ret = ilclient_change_component_state(image_decode, OMX_StateIdle);
     ret = ilclient_change_component_state(image_decode, OMX_StateLoaded);
     
     ilclient_cleanup_components(list);
     OMX_Deinit();
     ilclient_destroy(client);
     return status;
}

int main (int argc, char **argv)
{
     if (argc < 2) {
	  printf("Usage: %s <input filename>\n", argv[0]);
	  exit(1);
     }
     bcm_host_init();
     return image_decode_test(argv[1]);
}


