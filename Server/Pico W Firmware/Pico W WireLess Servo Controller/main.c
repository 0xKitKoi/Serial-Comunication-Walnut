#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/init.h"
#include "lwip/tcp.h"
#include "pico-servo/include/pico_servo.h" // https://github.com/irishpatrick/pico-servo

#define SERVER_IP "192.168.0.###"
#define SERVER_PORT 5000
#define WIFI_SSID ""
#define WIFI_PASSWORD ""
#define X_SERVO_PIN 16
#define Y_SERVO_PIN 17

struct tcp_pcb *tcp_client = NULL;
bool gracePeriod = true;

void close_tcp_client() {
    if (tcp_client != NULL) {
        tcp_arg(tcp_client, NULL);
        tcp_sent(tcp_client, NULL);
        tcp_recv(tcp_client, NULL);
        tcp_err(tcp_client, NULL);

        err_t err = tcp_close(tcp_client);
        if (err != ERR_OK) {
            // Handle error, if needed
        }

        tcp_client = NULL;
    }
}


uint map_angle_to_pwm(uint angle) {
    // Assume servo accepts angles between 0 to 180 degrees
    // and PWM signals between 1000 to 2000 microseconds
    // You may need to adjust these values based on your servo's specifications

    // Map the angle to the PWM signal range
    return 1000 + (angle * (2000 - 1000) / 180);
}

void parse_buffer(const char *buffer, uint *x_value, uint *y_value) {
    // Define delimiters and placeholders for extracted values
    const char *x_delimiter = "X";
    const char *y_delimiter = "Y";

    // Find X value
    const char *x_start = strstr(buffer, x_delimiter);
    if (x_start != NULL) {
        x_start += strlen(x_delimiter);
        *x_value = strtoul(x_start, NULL, 10);
    }

    // Find Y value
    const char *y_start = strstr(buffer, y_delimiter);
    if (y_start != NULL) {
        y_start += strlen(y_delimiter);
        *y_value = strtoul(y_start, NULL, 10);
    }
}

err_t tcp_client_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    if (tcp_client->state == 0) {
        // Handle connection closed or error
        printf("Connection closed or error.\n");
        close_tcp_client();
        return ERR_OK;
    }

    // Print the received data if pbuf and payload are not NULL
    if (p && p->payload) {
        printf("Received data from server: %.*s\n", p->tot_len, (char *)p->payload);

        char *tokens = strtok((char *)p->payload, "XY");
        bool switchServo = true;
	      uint x, y;
      	parse_buffer( (char*) p->payload, &x, &y);
      	printf("X Value: %u\n", x);
    	  printf("Y Value: %u\n", y);
      	servo_move_to(X_SERVO_PIN, x);        
	/*
	while (tokens != NULL) {
            printf("%s\n", tokens);
            int angle = atoi(tokens);

	    //uint angle = strtoul(tokens, NULL, 10);
	    //printf("Interpreted angle: %u",angle);
	    //angle = map_angle_to_pwm(angle);

	    //printf("Interpreted angle: %u",angle);
	    

	  */  /*
		
            if (switchServo) {
                servo_move_to(X_SERVO_PIN, angle);
            } else {
                servo_move_to(Y_SERVO_PIN, angle);
            }

            switchServo = !switchServo;
	    *//*
		


            servo_move_to(X_SERVO_PIN, angle);

            tokens = strtok(NULL, "XY");
        }
    */
    	
    } else {
        printf("Received data, but pbuf or payload is NULL.\n");
    }

    // Free the pbuf only if it is not NULL
    if (p) {
        pbuf_free(p);
    }

    return ERR_OK;
}

void create_and_connect_socket(ip_addr_t *server_ip, u16_t server_port) {
    close_tcp_client();
    tcp_client = tcp_new();

    tcp_recv(tcp_client, tcp_client_recv);
    gracePeriod = !gracePeriod;

    if (tcp_client == NULL) {
        printf("Failed to create TCP client.\n");
        return;
    }

    tcp_connect(tcp_client, server_ip, server_port, tcp_client_recv);
}

int connect_to_wifi() {
    printf("Connecting to Wi-Fi...\n");

    int retry_count = 0;
    while (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000) != 0) {
        printf("Failed to connect to Wi-Fi. Retrying (%d)...\n", ++retry_count);
        sleep_ms(2000);

        if (retry_count >= 5) {
            printf("Max retry count reached. Exiting.\n");
            return -1;
        }
    }

    printf("Connected to Wi-Fi.\n");
    return 0;
}

int main() {
    stdio_init_all();
    printf("Starting.\n");

    if (cyw43_arch_init()) {
        printf("Failed to initialize Wi-Fi. Exiting.\n");
        return 1;
    }

    cyw43_arch_enable_sta_mode();

    if (connect_to_wifi() != 0) {
        return -1;
    }

    // Initialize lwIP
    // lwip_init();

    ip_addr_t server_ip;
    IP4_ADDR(&server_ip, 192, 168, 0, 222);

    printf("Attempting to connect to the server.\n");
    create_and_connect_socket(&server_ip, SERVER_PORT);

    // Initialize the servo library
    servo_init();
    servo_clock_auto();
    servo_attach(X_SERVO_PIN);

    // Set servo pulse width bounds (adjust as needed)
    servo_set_bounds(1200, 8600);
            servo_move_to(X_SERVO_PIN, 0);
            sleep_ms(1000);
            servo_move_to(X_SERVO_PIN, 180);
            sleep_ms(1500);

    bool led = true;
    bool connected = false;
    while (1) {
        sys_check_timeouts();
        if (gracePeriod) {
            gracePeriod = !gracePeriod;
            sleep_ms(6000);
            continue;
        }
        if (tcp_client) {
            if (tcp_client->state > 0) {
		    if (!connected) {
                	printf("Connected to the server. | State: %u\n", tcp_client->state);
			connected = ! connected;
		    }
                cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led);
                led = !led;
                sleep_ms(600);
            } else {

			connected = ! connected;
                printf("Connection lost or not established. Reconnecting. %u\n", tcp_client->state);
                sleep_ms(1000);
                create_and_connect_socket(&server_ip, SERVER_PORT);
            }
        }
    }

    return 0;
}

