#include <linux/init.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/time.h>

#include <asm/irq.h>
#include <asm/io.h>

#define PIN 4
#define BAUD 10000
#define PIN_MAX 77
const int bit_dur = 1000000 / BAUD;
int gpio_irq;
struct timeval last;
bool receiving = false;
bool start_bit_handled;
int received_bits;
bool bits[8];

static struct input_dev *input_device;

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Emil Schätzle");
MODULE_DESCRIPTION("Driver for the PI-Book keyboard");
MODULE_VERSION("0.01");
static int get_time_difference(void)
{
    long long int time;
    struct timeval now;
    do_gettimeofday(&now);
    time = now.tv_sec - last.tv_sec;
    time *= 1000000;
    time += now.tv_usec - last.tv_usec;
    last = now;
    return time;
}
static int get_bit_durs(void)
{
    int time;
    int total_bit_durs;
    time = get_time_difference();
    total_bit_durs = (time + bit_dur / 2) / bit_dur;
    return total_bit_durs;
}
static int map_pin(int pin){
    switch (pin){
        case 0: return KEY_ESC;
        case 1: return KEY_F1;
        case 2: return KEY_F2;
        case 3: return KEY_F3;
        case 4: return KEY_F4;
        case 5: return KEY_F5;
        case 6: return KEY_F6;
        case 7: return KEY_F7;
        case 8: return KEY_F8;
        case 9: return KEY_F9;
        case 10: return KEY_F10;
        case 11: return KEY_F11;
        case 12: return KEY_F12;
        case 13: return KEY_DELETE;
        case 14: return KEY_GRAVE;
        case 15: return KEY_1;
        case 16: return KEY_2;
        case 17: return KEY_3;
        case 18: return KEY_4;
        case 19: return KEY_5;
        case 20: return KEY_6;
        case 21: return KEY_7;
        case 22: return KEY_8;
        case 23: return KEY_9;
        case 24: return KEY_0;
        case 25: return KEY_MINUS;
        case 26: return KEY_EQUAL;
        case 27: return KEY_BACKSPACE;
        case 28: return KEY_TAB;
        case 29: return KEY_Q;
        case 30: return KEY_W;
        case 31: return KEY_E;
        case 32: return KEY_R;
        case 33: return KEY_T;
        case 34: return KEY_Y;
        case 35: return KEY_U;
        case 36: return KEY_I;
        case 37: return KEY_O;
        case 38: return KEY_P;
        case 39: return KEY_LEFTBRACE;
        case 40: return KEY_RIGHTBRACE;
        case 41: return KEY_BACKSLASH;
        case 42: return KEY_CAPSLOCK;
        case 43: return KEY_A;
        case 44: return KEY_S;
        case 45: return KEY_D;
        case 46: return KEY_F;
        case 47: return KEY_G;
        case 48: return KEY_H;
        case 49: return KEY_J;
        case 50: return KEY_K;
        case 51: return KEY_L;
        case 52: return KEY_SEMICOLON;
        case 53: return KEY_APOSTROPHE;
        case 54: return KEY_ENTER;
        case 55: return KEY_LEFTSHIFT;
        case 56: return KEY_Z;
        case 57: return KEY_X;
        case 58: return KEY_C;
        case 59: return KEY_V;
        case 60: return KEY_B;
        case 61: return KEY_N;
        case 62: return KEY_M;
        case 63: return KEY_COMMA;
        case 64: return KEY_DOT;
        case 65: return KEY_SLASH;
        case 66: return KEY_RIGHTSHIFT;
        case 67: return KEY_LEFTCTRL;
        case 68: return KEY_102ND;
        case 69: return KEY_LEFTMETA;
        case 70: return KEY_LEFTALT;
        case 71: return KEY_SPACE;
        case 72: return KEY_RIGHTALT;
        case 73: return KEY_COMPOSE;
        case 74: return KEY_LEFT;
        case 75: return KEY_UP;
        case 76: return KEY_DOWN;
        case 77: return KEY_RIGHT;
        default: return -1;
    }
}
static void set_key_state(int key, int state){
    if(key != -1){
        input_report_key(input_device, key, state);
    }
}
static void key_down(int pin)
{
    printk(KERN_INFO "Keyboard: keydown\n");
    set_key_state(map_pin(pin), 1);
    input_sync(input_device);
}
static void key_up(int pin)
{
    printk(KERN_INFO "Keyboard: keyup\n");
    set_key_state(map_pin(pin), 0);
    input_sync(input_device);
}
static void data_received(void)
{
    int data = 0;
    data += 1 * bits[0];
    data += 2 * bits[1];
    data += 4 * bits[2];
    data += 8 * bits[3];
    data += 16 * bits[4];
    data += 32 * bits[5];
    data += 64 * bits[6];
    data += 128 * bits[7];
    // HANDLE RECEIVED DATA
    printk(KERN_INFO "Keyboard: %i\n", data);
    if (data <= 77) {
        key_down(data);
    } else if (data <= 155) {
        key_up(data - 78);
    } else if (data <= 224) {
       // tilt_x(data - 209);
    } else {
      // tilt_y(data - 240);
    }
}
static void rising(void)
{
    if (receiving)
    {
        // Data is currently beeing received
        int i;
        int bit_durs;
        bit_durs = get_bit_durs();
        // Ignore startbit
        if (!start_bit_handled && bit_durs > 0)
        {
            bit_durs--;
            start_bit_handled = true;
        }
        for (i = 0; i < bit_durs && i < 8; i++)
        {
            bits[received_bits + i] = false;
        }
        if (received_bits + bit_durs > 8)
        {
            // Error occured > abort receiving
            receiving = false;
        }
        else
        {
            received_bits += bit_durs;
            if (received_bits == 8)
            {
                receiving = false;
                data_received();
            }
        }
    }
}
static void falling(void)
{
    if (!receiving)
    {
        // New transmission starts, thus reset clock
        get_time_difference();
        receiving = true;
        received_bits = 0;
        start_bit_handled = false;
    }
    else
    {
        // Data is currently beeing received
        int i;
        int bit_durs;
        bit_durs = get_bit_durs();
        for (i = 0; i < bit_durs && i < 8; i++)
        {
            bits[received_bits + i] = true;
        }
        if (received_bits + bit_durs > 8)
        {
            // Error occured > abort receiving
            receiving = false;
        }
        else
        {
            received_bits += bit_durs;
            if (received_bits == 8)
            {
                receiving = false;
                data_received();
            }
        }
    }
}
static irq_handler_t interrupt(unsigned int irq, void *dev_id, struct pt_regs *regs)
{
    if (gpio_get_value(PIN))
    {
        rising();
    }
    else
    {
        falling();
    }
    return (irq_handler_t)IRQ_HANDLED;
}
static inline void register_key(int key){
    input_device->keybit[BIT_WORD(key)] |= BIT_MASK(key);
}
static int __init kb_mod_init(void)
{
    int result;
    int i;
    printk(KERN_INFO "Keyboard: Initialising on pin %i\n", PIN);
    gpio_request(PIN, "Keyboard pin");
    gpio_direction_input(PIN);
    printk(KERN_INFO "Keyboard: First value: %i\n", gpio_get_value(PIN));
    gpio_irq = gpio_to_irq(PIN);
    result = request_irq(gpio_irq, (irq_handler_t)interrupt, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, "Keyboard interrupt", NULL);
    printk(KERN_INFO "Keyboard: Initialising input device\n");
    input_device = input_allocate_device();
    input_device->evbit[0] = BIT_MASK(EV_KEY);
    input_device->evbit[0] |= BIT_MASK(EV_REP);
    input_device->name = "Pi-Book keyboard and joystick";
    input_device->id.bustype = BUS_RS232;
    input_device->id.version = 0;
    // Register all keys
    for (i = 0; i <= PIN_MAX; i++) {
        register_key(map_pin(i));
    }
    input_register_device(input_device);
    printk(KERN_INFO "Keyboard: Initialised\n");
    do_gettimeofday(&last);
    return 0;
}
static void __exit kb_mod_exit(void)
{
    printk(KERN_INFO "Keyboard: Releasing resources\n");
    free_irq(gpio_irq, NULL);
    gpio_free(PIN);    
    input_unregister_device(input_device);
    printk(KERN_INFO "Keyboard: Exit\n");
}
module_init(kb_mod_init);
module_exit(kb_mod_exit);