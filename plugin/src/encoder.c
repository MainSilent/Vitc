#include <psp2kern/io/stat.h>
#include <psp2kern/io/fcntl.h>
#include <psp2kern/kernel/sysmem.h>
#include <psp2kern/lowio/iftu.h>
#include <psp2kern/display.h>

#define WIDTH 480
#define HEIGHT 272

#define LOG_PATH "ux0:data/vitc/"
#define LOG_FILE LOG_PATH "log.txt"

#define ALIGN(x, a) (((x) + ((a) - 1)) & ~((a) - 1))
#define VIDEO_FRAME_SIZE_NV12(w, h) (((w) * (h) * 3) / 2)

#define SCE_DISPLAY_PIXELFORMAT_BGRA5551 0x50000000
#define MAX_FRAME_SIZE VIDEO_FRAME_SIZE_NV12(WIDTH, HEIGHT)

#define START_FRAME_CODE "G^kGtPhoMR0&Xj2k0z7P7@^0iM*#AL*UgzfEab$Gjhk@nzNGHse3sKHPW6U6KPqdrADB5p8KaEn9$Lq#LMyuata8fatqOj6Gd"
#define END_FRAME_CODE "@aejW9QqBnsR07eaUHy&MF7bEY#d2sG&Q7e6$bw^XWohJyH1ri8bdOUTpxJy2nu@q8e9HiFwZl*wanNFFPKS&DABtVpQjbBH2hd"


static SceUID log_fd = -1;
static SceUID frame_uid = -1;
static char *frame_addr;


static inline void LOG(char* msg) {
	if (log_fd > 0) {
		ksceIoWrite(log_fd, msg, strlen(msg));
		ksceDebugPrintf(msg);
	}
}


static inline unsigned int display_to_iftu_pixelformat(unsigned int fmt)
{
	switch (fmt) {
	case SCE_DISPLAY_PIXELFORMAT_A8B8G8R8:
	default:
		return SCE_IFTU_PIXELFORMAT_BGRX8888;
	case SCE_DISPLAY_PIXELFORMAT_BGRA5551:
		return SCE_IFTU_PIXELFORMAT_BGRA5551;
	}
}


static inline unsigned int display_pixelformat_bpp(unsigned int fmt)
{
	switch (fmt) {
	case SCE_DISPLAY_PIXELFORMAT_A8B8G8R8:
	default:
		return 4;
	case SCE_DISPLAY_PIXELFORMAT_BGRA5551:
		return 2;
	}
}


static inline int frame_convert_to_nv12(const SceDisplayFrameBufInfo *fb_info,
					int dst_width, int dst_height)
{
	uintptr_t dst_paddr;
	uintptr_t src_paddr = fb_info->paddr;
	unsigned int src_width = fb_info->framebuf.width;
	unsigned int src_width_aligned = ALIGN(src_width, 16);
	unsigned int src_pitch = fb_info->framebuf.pitch;
	unsigned int src_height = fb_info->framebuf.height;
	unsigned int src_pixelfmt = fb_info->framebuf.pixelformat;
	unsigned int src_pixelfmt_bpp = display_pixelformat_bpp(src_pixelfmt);

	static SceIftuCscParams RGB_to_YCbCr_JPEG_csc_params = {
		0, 0x202, 0x3FF,
		0, 0x3FF,     0,
		{
			{ 0x99, 0x12C,  0x3A},
			{0xFAA, 0xF57, 0x100},
			{0x100, 0xF2A, 0xFD7}
		}
	};

	ksceKernelGetPaddr(frame_addr, &dst_paddr);

	SceIftuConvParams params;
	memset(&params, 0, sizeof(params));
	params.size = sizeof(params);
	params.unk04 = 1;
	params.csc_params1 = &RGB_to_YCbCr_JPEG_csc_params;
	params.csc_params2 = NULL;
	params.csc_control = 1;
	params.unk14 = 0;
	params.unk18 = 0;
	params.unk1C = 0;
	params.alpha = 0xFF;
	params.unk24 = 0;

	SceIftuPlaneState src;
	memset(&src, 0, sizeof(src));
	src.fb.pixelformat = display_to_iftu_pixelformat(src_pixelfmt);
	src.fb.width = src_width_aligned;
	src.fb.height = src_height;
	src.fb.leftover_stride = (src_pitch - src_width_aligned) * src_pixelfmt_bpp;
	src.fb.leftover_align = 0;
	src.fb.paddr0 = src_paddr;
	src.unk20 = 0;
	src.src_w = (src_width * 0x10000) / dst_width;
	src.src_h = (src_height * 0x10000) / dst_height;
	src.dst_x = 0;
	src.dst_y = 0;
	src.src_x = 0;
	src.src_y = 0;

	SceIftuFrameBuf dst;
	memset(&dst, 0, sizeof(dst));
	dst.pixelformat = SCE_IFTU_PIXELFORMAT_NV12;
	dst.width = dst_width;
	dst.height = dst_height;
	dst.leftover_stride = 0;
	dst.leftover_align = 0;
	dst.paddr0 = dst_paddr;
	dst.paddr1 = dst_paddr + dst_width * dst_height;

	return ksceIftuCsc(&dst, &src, &params);
}


static inline int frame_init(unsigned int size)
{
	int ret;

	SceKernelAllocMemBlockKernelOpt opt;
	SceKernelMemBlockType type;
	SceKernelAllocMemBlockKernelOpt *optp;

	type = 0x10208006;
	size = ALIGN(size, 4 * 1024);
	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_PHYCONT |
			SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_ALIGNMENT;
	opt.alignment = 4 * 1024;
	optp = &opt;
	
	frame_uid = ksceKernelAllocMemBlock("frame_buffer", type, size, optp);
	if (frame_uid < 0) {
		LOG("Error allocating CSC dest memory\n");
		return frame_uid;
	}

	ret = ksceKernelGetMemBlockBase(frame_uid, (void **)&frame_addr);
	if (ret < 0) {
		LOG("Error getting CSC desr memory addr\n");
		ksceKernelFreeMemBlock(frame_uid);
		frame_uid = -1;
		return ret;
	}

	return 0;
}


static inline int frame_term()
{
	if (frame_uid >= 0) {
		ksceKernelFreeMemBlock(frame_uid);
		frame_uid = -1;
	}

	return 0;
}