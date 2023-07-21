#include <psp2kern/net/net.h>
#include <psp2kern/kernel/debug.h>
#include <psp2kern/kernel/sysclib.h>
#include <psp2kern/kernel/threadmgr.h>
#include <psp2kern/kernel/modulemgr.h> 
#include "encoder.c"

#define DEBUG 0
#define PORT 54102
#define BUFFER_SIZE 8160


SceUID net_thread_uid = 0;
SceUID addr_thread_uid = 0;
SceNetSockaddrIn client;
int socket = -1;
unsigned int client_len = sizeof(client);


int addr_thread(SceSize args, void *argp)
{
	char tmp;
	int ret = -1;

	while(1) {
		ret = ksceNetRecvfrom(socket, &tmp, sizeof(tmp), 0, (SceNetSockaddr *)&client, &client_len);
		if (ret < 0)
			LOG("Failed to get initial data from client\n");
	}
}


void send_frame() {
	int ret = -1;

	// Send start code
	ret = ksceNetSendto(socket, START_FRAME_CODE, sizeof(START_FRAME_CODE)-1, 0, (SceNetSockaddr *)&client, client_len);
	if (ret < 0) {
		LOG("Failed to send START_FRAME_CODE\n");
		return;
	}

	// Process frame
	SceDisplayFrameBufInfo fb_info;
	int head = ksceDisplayGetPrimaryHead();
	fb_info.size = sizeof(fb_info);
	fb_info.framebuf.pixelformat = SCE_DISPLAY_PIXELFORMAT_A8B8G8R8;
	ret = ksceDisplayGetProcFrameBufInternal(-1, head, 0, &fb_info);
	if (ret < 0 || fb_info.paddr == 0)
		ret = ksceDisplayGetProcFrameBufInternal(-1, head, 1, &fb_info);
	if (ret < 0)
		return;

	frame_term();
	ret = frame_init(MAX_FRAME_SIZE);
	if (ret < 0) {
		LOG("Error allocating the frame\n");
		return;
	}

	ret = frame_convert_to_nv12(&fb_info, WIDTH, HEIGHT);
	if (ret < 0) {
		LOG("Error sending NV12 frame\n");
		return;
	}

	// Send frame
	for (int i = 0; i < MAX_FRAME_SIZE / BUFFER_SIZE; i++) {
		char* chunk_frame = frame_addr + (i * BUFFER_SIZE);

		ret = ksceNetSendto(socket, chunk_frame, BUFFER_SIZE, 0, (SceNetSockaddr *)&client, client_len);
		if (ret < 0) {
			LOG("Failed to send frame\n");
			return;
		}

		ksceKernelDelayThread(1000);
	}

	// Send end code
	ret = ksceNetSendto(socket, END_FRAME_CODE, sizeof(END_FRAME_CODE)-1, 0, (SceNetSockaddr *)&client, client_len);
	if (ret < 0) {
		LOG("Failed to send END_FRAME_CODE\n");
		return;
	}
}


int net_thread(SceSize args, void *argp)
{
	int ret = -1;
    socket = ksceNetSocket("vitc_socket", SCE_NET_AF_INET, SCE_NET_SOCK_DGRAM, 0);
    if (socket < 0) {
        LOG("Failed to create socket\n");
        return SCE_KERNEL_START_SUCCESS;
    }

    SceNetSockaddrIn server;
    server.sin_family = SCE_NET_AF_INET;
    server.sin_port = ksceNetHtons(PORT);
    server.sin_addr.s_addr = SCE_NET_INADDR_ANY;

	LOG("Trying to bind the socket...\n");

	while(ret < 0) {
		ksceKernelDelayThread(3 * 1000 * 1000);

		ret = ksceNetBind(socket, (SceNetSockaddr *)&server, sizeof(server));
	}

    LOG("Binded Successfully\n");

	addr_thread_uid = ksceKernelCreateThread("addr_thread", addr_thread, 0x40, 0x10, 0, 0, 0);
	if (addr_thread_uid < 0) {
		LOG("Failed to create addr_thread\n");
		return SCE_KERNEL_START_SUCCESS;
	}

	ksceKernelStartThread(addr_thread_uid, 0, NULL);
	
	while(1) {
		if (client.sin_addr.s_addr != 0)
		{
			ksceDisplayWaitVblankStart();

			send_frame();
		}
		else {
			ksceKernelDelayThread(100 * 1000);
		}
	}

	return SCE_KERNEL_START_SUCCESS;
}


void _start() __attribute__ ((weak, alias("module_start")));
int module_start(SceSize args, void *argp)
{

	#if DEBUG == 1
		ksceIoMkdir(LOG_PATH, 6);
		log_fd = ksceIoOpen(LOG_FILE, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 6);
		if (log_fd < 0) {
			ksceIoWrite(log_fd, "Failed to Setup Debug", 22);
			return SCE_KERNEL_START_SUCCESS;
		}
		LOG("Vitc\n\n");
	#endif

	
	net_thread_uid = ksceKernelCreateThread("net_thread", net_thread, 0x40, 0x1000, 0, 0, 0);
	if (net_thread_uid < 0) {
		LOG("Failed to create net_thread\n");
		return SCE_KERNEL_START_SUCCESS;
	}

	ksceKernelStartThread(net_thread_uid, 0, NULL);

	return SCE_KERNEL_START_SUCCESS;
}
