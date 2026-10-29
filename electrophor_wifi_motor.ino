#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>

// -------------------------
// TB6612FNG
// -------------------------

#define PWMA 25
#define AIN1 26
#define AIN2 27
#define STBY 32

const int PWM_FREQ = 20000;
const int PWM_RESOLUTION = 8;

// Скорость в процентах: 0...100
int motor_speed = 50;

bool motor_running = false;


// -------------------------
// Wi-Fi
// -------------------------

const char *ssid = "ABOBA";
const char *password = "12345678";

WebServer server(80);


// -------------------------
// Управление мотором
// -------------------------

int speed_to_pwm(int percent)
{
    percent = constrain(percent, 0, 100);

    return map(percent, 0, 100, 0, 255);
}


void motor_stop()
{
    ledcWrite(PWMA, 0);

    digitalWrite(AIN1, LOW);
    digitalWrite(AIN2, LOW);

    digitalWrite(STBY, LOW);

    motor_running = false;

    Serial.println("Motor OFF");
}


void motor_start()
{
    digitalWrite(STBY, HIGH);

    // Вращение в одном направлении
    digitalWrite(AIN1, HIGH);
    digitalWrite(AIN2, LOW);

    int target_pwm = speed_to_pwm(motor_speed);

    // Плавный разгон
    for (int pwm = 0; pwm <= target_pwm; pwm += 5)
    {
        ledcWrite(PWMA, pwm);
        delay(20);
    }

    ledcWrite(PWMA, target_pwm);

    motor_running = true;

    Serial.print("Motor ON, speed = ");
    Serial.print(motor_speed);
    Serial.println("%");
}


void set_motor_speed(int percent)
{
    motor_speed = constrain(percent, 0, 100);

    if (motor_running)
    {
        ledcWrite(PWMA, speed_to_pwm(motor_speed));
    }

    Serial.print("Speed = ");
    Serial.print(motor_speed);
    Serial.println("%");
}


// -------------------------
// HTML
// -------------------------

String get_page()
{
    String page = R"rawliteral(
<!DOCTYPE html>
<html lang="ru">

<head>
    <meta charset="UTF-8">

    <meta name="viewport"
          content="width=device-width, initial-scale=1">

    <title>Электрофорная машина</title>

    <style>
        body {
            font-family: Arial, sans-serif;
            margin: 30px;
            text-align: center;
            background: #f5f5f5;
        }

        .page {
            max-width: 700px;
            margin: 0 auto;
            padding: 25px;
            background: white;
            border-radius: 16px;
        }

        h1 {
            margin-bottom: 8px;
        }

        h2 {
            margin-top: 0;
            font-weight: normal;
        }

        .authors {
            text-align: right;
            margin-top: 30px;
        }

        .cat {
            display: block;
            margin: 20px auto 30px auto;
            width: 300px;
            max-width: 90%;
            height: auto;
        }

        .status {
            font-size: 22px;
            margin: 25px 0;
        }

        .controls {
            margin-top: 20px;
        }

        button {
            width: 220px;
            height: 70px;
            margin: 10px;
            border: none;
            border-radius: 12px;
            font-size: 22px;
            cursor: pointer;
        }

        .on {
            background: #4CAF50;
            color: white;
        }

        .off {
            background: #F44336;
            color: white;
        }

        .speed-box {
            margin: 30px auto 10px auto;
            max-width: 450px;
        }

        .speed-value {
            font-size: 20px;
            font-weight: bold;
        }

        input[type="range"] {
            width: 100%;
            margin-top: 15px;
        }
    </style>
</head>

<body>

<div class="page">

    <h1>Электрофорная машина</h1>

    <h2>Проект по ПЦП</h2>

    <h3 class="authors">
        Малеева Софья, Сальникова Марина
    </h3>

    <img class="cat"
         src="/telepuz.jpg"
         alt="ТЕлепузик">

    <div class="status">
        Состояние:
        <b id="motorStatus">)rawliteral";

    page += motor_running ? "ВКЛЮЧЕН" : "ВЫКЛЮЧЕН";

    page += R"rawliteral(</b>
    </div>

    <div class="controls">
        <button class="on" onclick="motorOn()">
            ВКЛЮЧИТЬ
        </button>

        <button class="off" onclick="motorOff()">
            ВЫКЛЮЧИТЬ
        </button>
    </div>

    <div class="speed-box">
        <p>
            Скорость:
            <span class="speed-value" id="speedValue">)rawliteral";

    page += String(motor_speed);

    page += R"rawliteral(%</span>
        </p>

        <input type="range"
               id="speedSlider"
               min="0"
               max="100"
               value=")rawliteral";

    page += String(motor_speed);

    page += R"rawliteral("
               oninput="showSpeed(this.value)"
               onchange="sendSpeed(this.value)">
    </div>

    <img
    class="cat"
    src="https://raw.githubusercontent.com/demonsofya/electrophoretic-machine/fe1825bfe6c3248d714baa2baaeb4c878cb04492/tenor.gif"
    alt="Кот">

</div>


<script>

async function motorOn()
{
    const response = await fetch("/on");

    if (response.ok)
    {
        document.getElementById("motorStatus").innerText = "ВКЛЮЧЕН";
    }
}


async function motorOff()
{
    const response = await fetch("/off");

    if (response.ok)
    {
        document.getElementById("motorStatus").innerText = "ВЫКЛЮЧЕН";
    }
}


function showSpeed(value)
{
    document.getElementById("speedValue").innerText = value + "%";
}


async function sendSpeed(value)
{
    await fetch("/speed?value=" + value);
}

</script>

</body>
</html>
)rawliteral";

    return page;
}


// -------------------------
// HTTP handlers
// -------------------------

void handle_root()
{
    server.send(200, "text/html; charset=utf-8", get_page());
}


void handle_on()
{
    motor_start();

    server.send(200, "text/plain; charset=utf-8", "ON");
}


void handle_off()
{
    motor_stop();

    server.send(200, "text/plain; charset=utf-8", "OFF");
}


void handle_speed()
{
    if (!server.hasArg("value"))
    {
        server.send(400, "text/plain; charset=utf-8", "No value");
        return;
    }

    int value = server.arg("value").toInt();

    set_motor_speed(value);

    server.send(200, "text/plain; charset=utf-8", "OK");
}


void handle_image()
{
    File file = LittleFS.open("/telepuz.jpg", "r");

    if (!file)
    {
        server.send(404, "text/plain", "tenor.gif not found");
        return;
    }

    server.streamFile(file, "image/gif");

    file.close();
}


// -------------------------
// setup
// -------------------------

void setup()
{
    pinMode(AIN1, OUTPUT);
    pinMode(AIN2, OUTPUT);
    pinMode(STBY, OUTPUT);

    ledcAttach(PWMA, PWM_FREQ, PWM_RESOLUTION);

    // При включении мотор всегда выключен
    motor_stop();


    // LittleFS
    if (!LittleFS.begin(true))
    {
        Serial.println("LittleFS error");
    }
    else
    {
        Serial.println("LittleFS started");
    }


    // ESP32 создаёт собственную Wi-Fi сеть
        //------ wifi connecting
    Serial.begin(115200);

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    Serial.print("Connecting");

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }

    Serial.println();
    Serial.println("Connected!");

    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    Serial.begin(115200);

//===============================


    // Web server
    server.on("/", HTTP_GET, handle_root);
    server.on("/on", HTTP_GET, handle_on);
    server.on("/off", HTTP_GET, handle_off);
    server.on("/speed", HTTP_GET, handle_speed);
    server.on("/telepuz.jpg", HTTP_GET, handle_image);

    server.begin();

    Serial.println("Web server started");
}


// -------------------------
// loop
// -------------------------

void loop()
{
    server.handleClient();
}
