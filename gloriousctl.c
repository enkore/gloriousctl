#include <fcntl.h>
#include <getopt.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include <hidapi/hidapi.h>

#define MAX_STR 255

#define FREE(var) free(var); var = 0;

void hexDump (const char * desc, const void * addr, const int len);

struct supported_device {
    uint16_t vid;
    uint16_t pid;
    const char *name;
};

/* these mice are clearly based on sinowealth's design
 * and there are a whole bunch others
 * one of the easiest ways to figure out which, google for glorious mice
 * anti-cheat bans, and you'll find a bunch of similarly specced mice that
 * also got banned for using the same VID/PID.
 * some of these have screenshots of their control software, which clearly
 * is the same as the glorious software.
 */
static struct supported_device supported_devices[] = {
    { .vid = 0x258a, .pid = 0x33, .name =  "Glorious Model D" },
    { .vid = 0x258a, .pid = 0x36, .name =  "Glorious Model O/O-" }, // probably works
};

#pragma pack(push, 1)
typedef struct {
    uint8_t r, g, b;
} RGB8;

typedef struct {
    uint8_t r, b, g;
} RBG8;

enum rgb_effect {
    RGB_OFF = 0,
    RGB_GLORIOUS = 0x1,   /* unicorn mode */
    RGB_SINGLE = 0x2,     /* single constant color */
    RGB_BREATHING = 0x5,  /* RGB breathing */
    RGB_BREATHING7 = 0x3, /* breathing with seven colors */
    RGB_BREATHING1 = 0xa, /* single color breathing */
    RGB_TAIL = 0x4,       /* idk what this is supposed to be */
    RGB_RAVE = 0x7,       /* ig */
    RGB_WAVE = 0x9
};

static
const char *rgb_effect_to_name(enum rgb_effect rgb_effect)
{
    switch(rgb_effect) {
    case RGB_OFF: return "Off";
    case RGB_GLORIOUS: return "Glorious mode";
    case RGB_SINGLE: return "Single color";
    case RGB_BREATHING: return "RGB breathing";
    case RGB_BREATHING7: return "Seven-color breathing";
    case RGB_BREATHING1: return "Single color breathing";
    case RGB_TAIL: return "Tail effect";
    case RGB_RAVE: return "Two-color rave!11";
    case RGB_WAVE: return "Wave effect";
    default: return "control reaches end of non-void function";
    }
}

static
enum rgb_effect name_to_rgb_effect(const char *name)
{
    if(!strcmp(name, "glorious")) {
        return RGB_GLORIOUS;
    }
    if(!strcmp(name, "single")) {
        return RGB_SINGLE;
    }
    if(!strcmp(name, "breathing")) {
        return RGB_BREATHING;
    }
    if(!strcmp(name, "breathing7")) {
        return RGB_BREATHING7;
    }
    if(!strcmp(name, "breathing1")) {
        return RGB_BREATHING1;
    }
    if(!strcmp(name, "tail")) {
        return RGB_TAIL;
    }
    if(!strcmp(name, "rave")) {
        return RGB_RAVE;
    }
    if(!strcmp(name, "wave")) {
        return RGB_WAVE;
    }

    return RGB_OFF;
}

static
RGB8 int_to_rgb(unsigned int value)
{
    RGB8 rgb;
    rgb.r = value >> 16U;
    rgb.g = value >> 8U;
    rgb.b = value;
    return rgb;
}

static
RBG8 int_to_rbg(unsigned int value)
{
    RBG8 rgb;
    rgb.r = value >> 16U;
    rgb.g = value >> 8U;
    rgb.b = value;
    return rgb;
}

#define CMD_CONFIG 0x11
#define CONFIG_SIZE 520
#define CONFIG_SIZE_USED 131
#define REPORT_ID_CMD 0x5
#define REPORT_ID_CONFIG 0x4
#define XY_INDEPENDENT 0x80

struct config {
    uint8_t report_id;
    uint8_t command_id;
    uint8_t unk1;
    uint8_t config_write;
    /* 0x0 - read.
     * CONFIG_SIZE_USED-8 - write.
     */
    uint8_t unk2[6];
    uint8_t config1;
    /* 0x80 - XY DPI independent */
    uint8_t dpi_count:4;
    uint8_t active_dpi:4;
    uint8_t dpi_enabled;
    /* bit set: disabled, unset: enabled
     * this structure has support for eight DPI slots,
     * but the glorious software only exposes six
     */
    uint8_t dpi[16];
    /* DPI/CPI is encoded in the way the PMW3360 sensor accepts it
     * value = (DPI - 100) / 100
     * If XY are identical, dpi[0-6] contain the sensitivities,
     * while in XY independent mode each entry takes two chars for X and Y.
     */
    RGB8 dpi_color[8];

    uint8_t rgb_effect;
    /* see enum rgb_effect */

    uint8_t glorious_mode;
    /* 0x40 - brightness (constant)
     * 0x1/2/3 - speed
     */
    uint8_t glorious_direction;

    uint8_t single_mode;
    RBG8 single_color;

    uint8_t breathing7_mode;
    /* 0x40 - brightness (constant)
     * 0x1/2/3 - speed
     */
    uint8_t breathing7_colorcount;
    /* 7, constant */
    RBG8 breathing7_colors[7];

    uint8_t tail_mode;
    /* 0x10/20/30/40 - brightness
     * 0x1/2/3 - speed
     */

    uint8_t unk4[33];

    uint8_t rave_mode;
    /* 0x10/20/30/40 - brightness
     * 0x1/2/3 - speed
     */
    RBG8 rave_colors[2];

    uint8_t wave_mode;
    /* 0x10/20/30/40 - brightness
     * 0x1/2/3 - speed
     */

    uint8_t breathing1_mode;
    /* 0x1/2/3 - speed */
    RBG8 breathing1_color;

    uint8_t unk5;
    /* 0x1 - 2 mm
     * 0x2 - 3 mm
     */
    uint8_t lift_off_distance;
};

struct change_report {
    uint8_t report_id; /* = 7 */
    uint8_t unk1; /* always 1 */
    uint8_t active_dpi:4;
    uint8_t unk2:4; /* 6 */
    uint8_t dpi_x;
    uint8_t dpi_y;
    uint8_t unk3[3]; /* always 0 */
};
#pragma pack(pop)

static
int config_to_dpi(int config_value)
{
    return (config_value + 1) * 100;
}

static
int dpi_to_config(int dpi)
{
    return dpi / 100 - 1;
}

static
void print_color(RGB8 color)
{
    printf("\e[38;2;%d;%d;%dm", color.r, color.g, color.b);
    printf("#%02X%02X%02X", color.r, color.g, color.b);
    printf("\e[39m");
}

static
void print_color_rbg(RBG8 color)
{
    printf("\e[38;2;%d;%d;%dm", color.r, color.g, color.b);
    printf("#%02X%02X%02X", color.r, color.g, color.b);
    printf("\e[39m");
}

static
unsigned int clamp(unsigned int value, unsigned int lower, unsigned int upper)
{
    if(value < lower) {
        return lower;
    }
    if(value > upper) {
        return upper;
    }

    return value;
}

static
void dump_config(const struct config *cfg)
{
    int xy_independent = (cfg->config1 & XY_INDEPENDENT) == XY_INDEPENDENT;
    printf("XY DPI independent: %s\n", xy_independent ? "yes" : "no");
    for(unsigned int i = 0; i < 6; i++) {
        if(cfg->dpi_enabled & (1U<<i)) {
            printf("[ ] ");
        } else {
            if(cfg->active_dpi == i) {
                printf("\e[1m[x]\e[0m ");
            } else {
                printf("[x] ");
            }
        }
        printf("DPI setting %d: ", i+1);
        if(xy_independent) {
            printf("%d/%d DPI\t", config_to_dpi(cfg->dpi[i*2]), config_to_dpi(cfg->dpi[i*2+1]));
        } else {
            printf("%d DPI\t", config_to_dpi(cfg->dpi[i]));
        }
        print_color(cfg->dpi_color[i]);
        printf("\n");
    }

    printf("\nRGB mode: %s\n", rgb_effect_to_name(cfg->rgb_effect));
}

static
char *find_device(const struct supported_device *dev)
{
    struct hid_device_info *devices = hid_enumerate(dev->vid, dev->pid);
    struct hid_device_info *device = devices;
    char *path = NULL;
    while(device) {
        if(device->interface_number == 1) {
            path = strdup(device->path);
            break;
        }
        device = device->next;
    }

    hid_free_enumeration(devices);
    return path;
}

static
char *detect_device()
{
    hid_init();

    char *path = NULL;
    for(unsigned int i = 0; i < sizeof(supported_devices)/sizeof(supported_devices[0]); i++) {
        path = find_device(&supported_devices[i]);
        if(path) {
            fprintf(stderr, "Detected %s\n", supported_devices[i].name);
            return path;
        }
    }
    return NULL;
}

static
void print_hid_error(hid_device *handle, const char *operation)
{
    char err[200];
    const wchar_t *werr = hid_error(handle);
    if(werr) {
        wcstombs(err, werr, sizeof(err));
    } else {
        strcpy(err, "Unknown error");
    }
    fprintf(stderr, "%s: %s\n", operation, err);
}

static
int print_firmware_version(hid_device *dev)
{
    int res = 0;
    uint8_t version[6] = {0x5, 0x1};
    res = hid_send_feature_report(dev, version, sizeof(version));
    if(res != sizeof(version)) {
        print_hid_error(dev, "get firmware version command");
        return 1;
    }
    res = hid_get_feature_report(dev, version, sizeof(version));
    if(res != sizeof(version)) {
        print_hid_error(dev, "read firmware version");
        return 1;
    }
    printf("Firmware version: %.4s\n", version + 2);

    return 0;
}

static
int print_help()
{
    fprintf(stderr,
            "gloriousctl\n"
            "A utility to adjust the settings of Model O/D mice\n"
            "Copyright (c) 2020 Marian Beermann under the EUPL license\n"
            "\n"
            "Usage:\n"
            " gloriousctl --help\n"
            "\tShow this help text.\n"
            " gloriousctl --info\n"
            "\tShow the current configuration of the mouse.\n"
            " gloriousctl --listen\n"
            "\tListen for and show DPI profile changes.\n"
            " gloriousctl [--set-...]\n"
            "\tChange persistent mouse settings.\n"
            "\n"
            "Available settings:\n"
            " --set-dpi DPI1,...\n"
            "\tUp to six DPIs can be configured.\n"
            " --set-dpi-color RRGGBB,...\n"
            "\tFor each DPI the RGB color can be set.\n"
            " --set-effect effect-name\n"
            "\tAvailable RGB effects: off, glorious, breathing, wave, tail,\n"
            "\tsingle, breathing7, breathing1, rave\n"
            "\tsingle and breathing1 use one color, breathing7 seven, rave two.\n"
            " --set-colors RRGGBB,...\n"
            "\tSet the color(s) of the effect. Only effective with --set-effect.\n"
            " --set-brightness 0-4\n"
            "\tSet the brightness of the effect. Only effective with --set-effect.\n"
            " --set-speed 0-3\n"
            "\tSet the speed of the effect. Only effective with --set-effect.\n"
            "\n"
        );

    fprintf(stderr, "Supported mice:\n");
    for(unsigned int i = 0; i < sizeof(supported_devices)/sizeof(supported_devices[0]); i++) {
        struct supported_device dev = supported_devices[i];
        fprintf(stderr, " - %s (VID %04x PID %04x)\n", dev.name, dev.vid, dev.pid);
    }

    return 0;
}

int main(int argc, char* argv[])
{
    if(argc == 1)
        return print_help();

    int do_info = 0;
    int do_help = 0;
    int do_set = 0;
    int do_listen = 0;

    const char *set_dpi = 0;
    const char *set_dpi_color = 0;
    const char *set_effect = 0;
    const char *set_effect_colors = "";
    const char *set_effect_brightness = "4";
    const char *set_effect_speed = "3";

    struct option options[] = {
        {"info", no_argument, &do_info, 1},
        {"help", no_argument, &do_help, 1},
        {"listen", no_argument, &do_listen, 1},
        {"set-dpi", required_argument, 0, 'a'},
        {"set-dpi-color", required_argument, 0, 'b'},
        {"set-effect", required_argument, 0, 'c'},
        {"set-colors", required_argument, 0, 'd'},
        {"set-brightness", required_argument, 0, 'e'},
        {"set-speed", required_argument, 0, 'f'},
        {0}
    };
    while(1) {
        int c = getopt_long(argc, argv, "", options, NULL);
        if(c == -1)
            break;
        if(!c)
            continue;
        if(c == '?')
            return 1;

        do_set = 1;
        switch(c) {
        case 'a':
            set_dpi = optarg;
            break;
        case 'b':
            set_dpi_color = optarg;
            break;
        case 'c':
            set_effect = optarg;
            break;
        case 'd':
            set_effect_colors = optarg;
            break;
        case 'e':
            set_effect_brightness = optarg;
            break;
        case 'f':
            set_effect_speed = optarg;
            break;
        default:;
        }
    }

    if(do_help) {
        return print_help();
    }
    if(do_info || do_set || do_listen) {
        int res = 0;
        char *dev_path = detect_device();
        hid_device *dev = NULL;

        if(!dev_path) {
            fprintf(stderr, "No supported device found.\n");
            return 0;
        }
        fprintf(stderr, "Opening device %s\n", dev_path);
        dev = hid_open_path(dev_path);
        FREE(dev_path);
        if(!dev) {
            fprintf(stderr, "Failed to open HID device. Try sudo.\n");
            return 1;
        }

        printf("%lu\n", sizeof(struct config));

        res = print_firmware_version(dev);
        if (res) {
            return res;
        }

        uint8_t cmd[6] = {REPORT_ID_CMD, CMD_CONFIG};
        res = hid_send_feature_report(dev, cmd, sizeof(cmd));
        if(res != sizeof(cmd)) {
            print_hid_error(dev, "get config command");
            return 1;
        }

        struct config *cfg = calloc(1, CONFIG_SIZE);
        cfg->report_id = REPORT_ID_CONFIG;
        /* this isn't portable code anyway due to packed structs */
        res = hid_get_feature_report(dev, (uint8_t*)cfg, CONFIG_SIZE);
        if(res == -1) {
            print_hid_error(dev, "read config");
            return 1;
        }
        printf("read cfg: %d bytes\n", res);

        hexDump("config", cfg, res);

        if(do_info) {
            dump_config(cfg);
        } else if(do_listen) {
            /* obviously this is kinda pointless in a CLI tool;
             * consider it sample code if someone wants to write some kind of
             * LGS (Logi... eh... Linux Gaming Software) that supports a bunch of gaming hardware
             */
            struct change_report report;
            while(1) {
                res = hid_read_timeout(dev, (uint8_t*)&report, sizeof(report), -1);
                if(res != sizeof(report)) {
                    print_hid_error(dev, "read input report");
                    return 1;
                }
                printf("Active profile: %d, X DPI: %d, Y DPI: %d\n",
                       report.active_dpi, config_to_dpi(report.dpi_x), config_to_dpi(report.dpi_y));
                hexDump("inpr", &report, sizeof(report));
            }
        } else if(do_set) {
            if(set_dpi) {
                int dpi[6] = {0};
                unsigned int num_dpis = sscanf(set_dpi, "%d,%d,%d,%d,%d,%d", &dpi[0], &dpi[1], &dpi[2],
                                      &dpi[3], &dpi[4], &dpi[5]);
                cfg->dpi_enabled = 0xff;
                cfg->dpi_count = 0;
                for(unsigned int i = 0; i < num_dpis; i++) {
                    cfg->dpi[i] = dpi_to_config(dpi[i]);
                    cfg->dpi_enabled &= ~(1U<<i);
                    cfg->dpi_count++;
                }
                printf("%s %d\n", set_dpi, num_dpis);
            }
            if(set_dpi_color) {
                unsigned int dpi_color[6] = {0};
                unsigned int num_colors = sscanf(set_dpi_color, "%x,%x,%x,%x,%x,%x", &dpi_color[0], &dpi_color[1],
                                        &dpi_color[2], &dpi_color[3], &dpi_color[4], &dpi_color[5]);
                for(unsigned int i = 0; i < num_colors; i++) {
                    cfg->dpi_color[i] = int_to_rgb(dpi_color[i]);
                }
            }
            if(set_effect) {
                cfg->rgb_effect = name_to_rgb_effect(set_effect);
                unsigned int rgb_colors[7] = {0};
                unsigned int num_colors = sscanf(set_effect_colors,  "%x,%x,%x,%x,%x,%x,%x", &rgb_colors[0],
                                        &rgb_colors[1], &rgb_colors[2], &rgb_colors[3], &rgb_colors[4],
                                        &rgb_colors[5], &rgb_colors[6]);
                switch(cfg->rgb_effect) {
                case RGB_SINGLE:
                    cfg->single_color = int_to_rbg(rgb_colors[0]);
                    break;
                case RGB_BREATHING7:
                    for(unsigned int i = 0; i < num_colors; i++) {
                        cfg->breathing7_colors[i] = int_to_rbg(rgb_colors[i]);
                    }
                    break;
                case RGB_BREATHING1:
                    cfg->breathing1_color = int_to_rbg(rgb_colors[0]);
                    break;
                case RGB_RAVE:
                    cfg->rave_colors[0] = int_to_rbg(rgb_colors[0]);
                    cfg->rave_colors[0] = int_to_rbg(rgb_colors[1]);
                }

                unsigned int brightness = 4;
                unsigned int speed = 3;
                sscanf(set_effect_brightness, "%d", &brightness);
                sscanf(set_effect_speed, "%d", &speed);
                brightness = clamp(brightness, 0, 4);
                speed = clamp(speed, 0, 3);
                uint8_t mode = speed | (brightness << 4U);

                switch(cfg->rgb_effect) {
                case RGB_GLORIOUS:
                    cfg->glorious_mode = mode;
                    break;
                case RGB_SINGLE:
                    cfg->single_mode = mode;
                    break;
                case RGB_BREATHING7:
                    cfg->breathing7_mode = mode;
                    break;
                case RGB_BREATHING1:
                    cfg->breathing1_mode = mode;
                    break;
                case RGB_TAIL:
                    cfg->tail_mode = mode;
                    break;
                case RGB_RAVE:
                    cfg->rave_mode = mode;
                    break;
                case RGB_WAVE:
                    cfg->wave_mode = mode;
                    break;
                }
            }

            dump_config(cfg);

            cfg->config_write = CONFIG_SIZE_USED - 8;

            res = hid_send_feature_report(dev, (uint8_t*)cfg, CONFIG_SIZE);
            if(res == -1) {
                print_hid_error(dev, "write config");
                return 1;
            }
            printf("%d\n", res);
        }

        hid_close(dev);
        hid_exit();
    }
    return 0;
}


/* from https://stackoverflow.com/a/7776146/675646 */
void hexDump (const char * desc, const void * addr, const int len) {
    int i = 0;
    unsigned char buff[17];
    const unsigned char * pc = (const unsigned char *)addr;

    // Output description if given.

    if (desc != NULL)
        printf ("%s:\n", desc);

    // Length checks.

    if (len == 0) {
        printf("  ZERO LENGTH\n");
        return;
    }
    if (len < 0) {
        printf("  NEGATIVE LENGTH: %d\n", len);
        return;
    }

    // Process every byte in the data.

    for (i = 0; i < len; i++) {
        // Multiple of 16 means new line (with line offset).

        if ((i % 16) == 0) {
            // Don't print ASCII buffer for the "zeroth" line.

            if (i != 0)
                printf ("  %s\n", buff);

            // Output the offset.

            printf ("  %04x ", i);
        }

        // Now the hex code for the specific character.
        printf (" %02x", pc[i]);

        // And buffer a printable ASCII character for later.

        if ((pc[i] < 0x20) || (pc[i] > 0x7e)) // isprint() may be better.
            buff[i % 16] = '.';
        else
            buff[i % 16] = pc[i];
        buff[(i % 16) + 1] = '\0';
    }

    // Pad out last line if not exactly 16 characters.

    while ((i % 16) != 0) {
        printf ("   ");
        i++;
    }

    // And print the final ASCII buffer.

    printf ("  %s\n", buff);
}
