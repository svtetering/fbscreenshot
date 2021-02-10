#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdlib.h>
#include <linux/fb.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

void write_bmp_header(char* buffer, int width, int height, int bytes_per_pixel, size_t image_size) {
    size_t file_size = image_size + 52;

    // Set all bytes in the header to 0
    memset(buffer, 0, 52);

    // Magic number: BM
    buffer[0x00] = 0x42;
    buffer[0x01] = 0x4D;

    // File size
    char* file_size_header = &buffer[2];
    memcpy(file_size_header, (uint32_t*)&file_size, sizeof(uint32_t));

    // Pixel data starting address
    buffer[0x0A] = 0x36;

    // Start of DIB header BITMAPINFOHEADER
    buffer[0x0E] = 40;
    
    char* image_width_dib = &buffer[0x12];
    char* image_height_dib = &buffer[0x16];
    memcpy(image_width_dib, &width, sizeof(int));
    memcpy(image_height_dib, &height, sizeof(int));

    buffer[0x1A] = 1;
    buffer[0x1C] = bytes_per_pixel * 8;
    buffer[0x1E] = 0;

    char* image_size_dib = &buffer[0x22];
    memcpy(image_size_dib, (uint32_t*)&image_size, sizeof(uint32_t));

    char* horizontal_image_res_dib = &buffer[0x26];
    char* vertical_image_res_dib = &buffer[0x2A];
    int horizontal_image_res = 0;
    int vertical_image_res = 0;
    memcpy(horizontal_image_res_dib, &horizontal_image_res, sizeof(int));
    memcpy(vertical_image_res_dib, &vertical_image_res, sizeof(int));

    char* num_colors_dib = &buffer[0x2E];
    char* num_imp_colors_dib = &buffer[0x32];
    uint32_t num_colors = 0;
    uint32_t num_imp_colors = 0;
    memcpy(num_colors_dib, &num_colors, sizeof(uint32_t));
    memcpy(num_colors_dib, &num_imp_colors, sizeof(uint32_t));
}

void read_framebuffer_pixels(char* pixel_data, int framebuffer_fd, struct fb_var_screeninfo vinfo, int bytes_per_pixel, int padding_bytes) {
    // Skip vinfo.yoffset rows
    lseek(framebuffer_fd, vinfo.yoffset * vinfo.xres_virtual * bytes_per_pixel, SEEK_CUR);
    for (int y = 0; y < vinfo.yres; y++) {
        // Skip vinfo.xoffset columns
        lseek(framebuffer_fd, vinfo.xoffset * bytes_per_pixel, SEEK_CUR);

        char* row = pixel_data + y * vinfo.xres * bytes_per_pixel + y * padding_bytes;
        int succ = read(framebuffer_fd, row, vinfo.xres * bytes_per_pixel);
        if (succ == -1) {
            perror("read");
            exit(1);
        }

        // Skip horizontal part of framebuffer that isn't shown on screen
        lseek(framebuffer_fd, (vinfo.xres_virtual - vinfo.xres - vinfo.xoffset) * bytes_per_pixel, SEEK_CUR);
    }
}

int main(int argc, char** argv) {
    char* framebuffer_path = "/dev/fb0";
    char* output_path = "output.bmp";
    bool capture_full_virtual_framebuffer = false;

    for (int i = 1; i < argc; i++) {
        char* argument = argv[i];
        if (strcmp(argument, "--help") == 0) {
            printf("USAGE: fbscreenshot [OPTIONS]\nOPTIONS:\n --help    - Show this help message\n -f [path] - The path of the framebuffer to read from\n -o [path] - The path of the bitmap file that is exported\n -v        - Copy full virtual framebuffer\n");
            return 0;
        }
        if (strcmp(argument, "-f") == 0) {
            if (i + 1 == argc) {
                fprintf(stderr, "-f: No path given\n");
                return 1;
            }
            i++;
            framebuffer_path = argv[i];
        } else if (strcmp(argument, "-o") == 0) {
            if (i + 1 == argc) {
                fprintf(stderr, "-o: No path given\n");
                return 1;
            }
            i++;
            output_path = argv[i];
        } else if (strcmp(argument, "-v") == 0) {
            capture_full_virtual_framebuffer = true;
        }
    }

    int framebuffer_fd = open(framebuffer_path, O_RDONLY);
    if (framebuffer_fd == -1) {
        perror("open");
        return 1;
    }

    // Get information about the framebuffer using ioctl()
    struct fb_var_screeninfo vinfo;
    int succ = ioctl(framebuffer_fd, FBIOGET_VSCREENINFO, &vinfo);
    if (succ == -1) {
        perror("ioctl");
        return 1;
    }

    int capture_width = capture_full_virtual_framebuffer ? vinfo.xres_virtual : vinfo.xres;
    int capture_height = capture_full_virtual_framebuffer ? vinfo.yres_virtual : vinfo.yres;
    int bytes_per_pixel = vinfo.bits_per_pixel / 8;
    int padding_bytes = (4 - (capture_width * bytes_per_pixel) % 4) % 4;
    
    size_t image_size = capture_width * capture_height * bytes_per_pixel + padding_bytes * capture_height;
    size_t file_size = image_size + 54;
    char data[file_size];
    write_bmp_header(&data[0], capture_width, -capture_height, bytes_per_pixel, image_size);

    if (capture_full_virtual_framebuffer) {
        // Adjust boundaries so read_framebuffer_pixels() reads the whole framebuffer
        vinfo.xoffset = 0;
        vinfo.yoffset = 0;
        vinfo.xres = vinfo.xres_virtual;
        vinfo.yres = vinfo.yres_virtual;
    }
    char* pixel_data = &data[0x36];
    read_framebuffer_pixels(pixel_data, framebuffer_fd, vinfo, bytes_per_pixel, padding_bytes);

    int output_fd = open(output_path, O_WRONLY | O_CREAT, 0644);
    if (output_fd == -1) {
        perror("open");
        return 1;
    }
    write(output_fd, data, sizeof(data));

    close(framebuffer_fd);
    close(output_fd);
}