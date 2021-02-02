#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
#include <linux/fb.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

const char* framebuffer_path = "/dev/fb0";
const char* output_path = "output.bmp";
const bool capture_full_virtual_framebuffer = false;

int main(int argc, char** argv) {
    int framebuffer_fd = open(framebuffer_path, O_RDONLY);
    if (framebuffer_fd == -1) {
        perror("open");
        return 1;
    }

    struct fb_var_screeninfo vinfo;
    int succ = ioctl(framebuffer_fd, FBIOGET_VSCREENINFO, &vinfo);
    if (succ == -1) {
        perror("ioctl");
        return 1;
    }

    int capture_width, capture_height;
    if (capture_full_virtual_framebuffer) {
        capture_width = vinfo.xres_virtual;
        capture_height = vinfo.yres_virtual;
    } else {
        capture_width = vinfo.xres;
        capture_height = vinfo.yres;
    }
    int bytes_per_pixel = vinfo.bits_per_pixel / 8;

    size_t file_size = capture_width * capture_height * bytes_per_pixel;

    char data[25 + file_size];
    memset(&data[0], 0, file_size);

    data[0x00] = 0x42;
    data[0x01] = 0x4D;
    char* file_size_header = &data[2];
    memcpy(file_size_header, (uint32_t*)&file_size, sizeof(uint32_t));

    data[0x0A] = 0x25;
    data[0x0E] = 12;
    
    char* image_width_dib = &data[0x12];
    char* image_height_dib = &data[0x14];
    memcpy(image_width_dib, &capture_width, sizeof(uint16_t));
    int flipped_image_height = -capture_height;
    memcpy(image_height_dib, &flipped_image_height, sizeof(uint16_t));

    data[0x16] = 1;
    data[0x18] = 8 * bytes_per_pixel;

    char* pixel_data = &data[0x25];

    if (capture_full_virtual_framebuffer) {
        succ = read(framebuffer_fd, pixel_data, file_size);
        if (succ == -1) {
            perror("read");
            return 1;
        }
    } else {
        for (int y = 0; y < vinfo.yres; y++) {
            succ = read(framebuffer_fd, pixel_data + y * vinfo.xres * bytes_per_pixel, vinfo.xres * bytes_per_pixel);
            if (succ == -1) {
                perror("read");
                return 1;
            }

            lseek(framebuffer_fd, (vinfo.xres_virtual - vinfo.xres) * bytes_per_pixel, SEEK_CUR);
        }
    }

    int output_fd = open(output_path, O_WRONLY | O_CREAT);
    write(output_fd, data, sizeof(data));

    close(framebuffer_fd);
    close(output_fd);
}