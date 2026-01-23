#define BLINKER_BLE

#include <Blinker.h>
#include <Adafruit_NeoPixel.h>

// ================= 配置区域 =================
// 注意：蓝牙模式下不需要密钥和WiFi信息
// 请在Blinker App中添加设备 -> 选择“蓝牙接入” -> 搜索连接本设备

// WS2812 灯珠配置
// 建议引脚：ESP32-C3 常用 GPIO 4, 5, 6 (请根据实际接线修改)
#define LED_PIN_1 1
#define LED_PIN_2 2
#define LED_PIN_3 3

#define LED_COUNT 225     // 每个引脚控制的灯珠数量

// ================= LED 矩阵映射与字体 =================
// 15x15 矩阵映射数组 (Zigzag 排列)
// 如果您的灯板排列方式不同，请修改此数组
const int LED_MAP[15][15] = {
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14},
    {29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15},
    {30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44},
    {59, 58, 57, 56, 55, 54, 53, 52, 51, 50, 49, 48, 47, 46, 45},
    {60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74},
    {89, 88, 87, 86, 85, 84, 83, 82, 81, 80, 79, 78, 77, 76, 75},
    {90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104},
    {119, 118, 117, 116, 115, 114, 113, 112, 111, 110, 109, 108, 107, 106, 105},
    {120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134},
    {149, 148, 147, 146, 145, 144, 143, 142, 141, 140, 139, 138, 137, 136, 135},
    {150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164},
    {179, 178, 177, 176, 175, 174, 173, 172, 171, 170, 169, 168, 167, 166, 165},
    {180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194},
    {209, 208, 207, 206, 205, 204, 203, 202, 201, 200, 199, 198, 197, 196, 195},
    {210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 224},
};

// 3x5 数字字体 (0-9)
const uint8_t DIGITS[10][5] = {
    {0x07, 0x05, 0x05, 0x05, 0x07}, // 0
    {0x02, 0x02, 0x02, 0x02, 0x02}, // 1
    {0x07, 0x01, 0x07, 0x04, 0x07}, // 2
    {0x07, 0x01, 0x07, 0x01, 0x07}, // 3
    {0x05, 0x05, 0x07, 0x01, 0x01}, // 4
    {0x07, 0x04, 0x07, 0x01, 0x07}, // 5
    {0x07, 0x04, 0x07, 0x05, 0x07}, // 6
    {0x07, 0x01, 0x01, 0x01, 0x01}, // 7
    {0x07, 0x05, 0x07, 0x05, 0x07}, // 8
    {0x07, 0x05, 0x07, 0x01, 0x07}  // 9
};

// ===========================================

// 初始化 NeoPixel 对象
Adafruit_NeoPixel strip1(LED_COUNT, LED_PIN_1, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel strip2(LED_COUNT, LED_PIN_2, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel strip3(LED_COUNT, LED_PIN_3, NEO_GRB + NEO_KHZ800);

// 创建组件对象
BlinkerSlider SliderR("range-r"); // 红色滑块
BlinkerSlider SliderG("range-g"); // 绿色滑块
BlinkerSlider SliderB("range-b"); // 蓝色滑块
BlinkerText TextTime("tex-time"); // 倒计时显示
BlinkerSlider SliderTime("set-time"); // 设置倒计时时长 (秒)
BlinkerButton BtnStart("btn-start");  // 开始/停止倒计时按钮

// 全局变量存储当前RGB值
int currentR = 0;
int currentG = 10;
int currentB = 5;

// 倒计时相关变量
enum Mode {
    MODE_NORMAL,
    MODE_COUNTDOWN,
    MODE_BREATHING
};
Mode currentMode = MODE_NORMAL;
int countdownSeconds = 0;           // 设定的倒计时秒数
unsigned long countdownEndTime = 0; // 倒计时结束的时间戳

// ================= 矩阵显示辅助函数 =================

/**
 * @brief 在矩阵指定坐标绘制像素
 */
void drawPixel(int x, int y, uint32_t color) {
    if (x < 0 || x >= 15 || y < 0 || y >= 15) return;
    int index = LED_MAP[y][x];
    if (index >= 0 && index < LED_COUNT) {
        strip1.setPixelColor(index, color);
        strip2.setPixelColor(index, color);
        strip3.setPixelColor(index, color);
    }
}

/**
 * @brief 绘制单个数字
 * @param x 起始X坐标
 * @param y 起始Y坐标
 * @param digit 要显示的数字 (0-9)
 * @param color 颜色
 */
void drawDigit(int x, int y, int digit, uint32_t color) {
    if (digit < 0 || digit > 9) return;
    for (int dy = 0; dy < 5; dy++) {
        for (int dx = 0; dx < 3; dx++) {
            // 检查字体位图 (从高位到低位: bit 2, 1, 0)
            if ((DIGITS[digit][dy] >> (2 - dx)) & 0x01) {
                drawPixel(x + dx, y + dy, color);
            }
        }
    }
}

/**
 * @brief 清除矩阵显示
 */
void clearMatrix() {
    // 仅清除缓冲区，不立即刷新，避免闪烁
    strip1.clear();
    strip2.clear();
    strip3.clear();
}

/**
 * @brief 在矩阵上显示时间 (MM SS)
 */
void showTimeOnMatrix(int minutes, int seconds) {
    clearMatrix();
    
    // 确定颜色，如果全黑则使用默认白色
    uint32_t color;
    if (currentR == 0 && currentG == 0 && currentB == 0) {
        color = strip1.Color(50, 50, 50);
    } else {
        color = strip1.Color(currentR, currentG, currentB);
    }

    // 分钟 (上排)
    int min_tens = minutes / 10;
    int min_units = minutes % 10;
    drawDigit(4, 2, min_tens, color); // X=4, Y=2
    drawDigit(8, 2, min_units, color); // X=8, Y=2

    // 秒数 (下排)
    int sec_tens = seconds / 10;
    int sec_units = seconds % 10;
    drawDigit(4, 9, sec_tens, color); // X=4, Y=9
    drawDigit(8, 9, sec_units, color); // X=8, Y=9

    strip1.show();
    strip2.show();
    strip3.show();
}

// ==================================================

// 辅助函数：更新所有灯带颜色
void setAllStripsColor(uint32_t color) {
    for(int i = 0; i < LED_COUNT; i++) {
        strip1.setPixelColor(i, color);
        strip2.setPixelColor(i, color);
        strip3.setPixelColor(i, color);
    }
    strip1.show();
    strip2.show();
    strip3.show();
}

/**
 * @brief 更新LED颜色
 * 将当前的RGB值应用到所有灯带上 (仅在非呼吸模式下生效)
 */
void updateColor() {
    if (currentMode == MODE_BREATHING) return; // 呼吸模式下不响应滑块直接变色

    uint32_t color = strip1.Color(currentR, currentG, currentB);
    setAllStripsColor(color);
    
    BLINKER_LOG("Color Updated: R=", currentR, ", G=", currentG, ", B=", currentB);
}

/**
 * @brief 格式化剩余时间
 */
String getRemainingTimeStr() {
    if (currentMode != MODE_COUNTDOWN) return "停止";
    
    long remaining = (long)(countdownEndTime - millis()) / 1000;
    if (remaining < 0) return "00:00";
    
    int rem_min = remaining / 60;
    int rem_sec = remaining % 60;
    
    char time_str[20];
    sprintf(time_str, "%02d:%02d", rem_min, rem_sec);
    return String(time_str);
}

/**
 * @brief 心跳包回调函数
 */
void heartbeat() {
    if (currentMode == MODE_COUNTDOWN) {
        TextTime.print(getRemainingTimeStr());
        BtnStart.print("on"); // 按钮显示为开启状态
        BtnStart.text("停止倒计时");
    } else if (currentMode == MODE_BREATHING) {
        TextTime.print("时间到!");
        BtnStart.print("on");
        BtnStart.text("停止呼吸灯");
    } else {
        TextTime.print("未开始");
        BtnStart.print("off");
        BtnStart.text("开始倒计时");
    }
    
    // 同步RGB滑块状态
    SliderR.print(currentR);
    SliderG.print(currentG);
    SliderB.print(currentB);
    SliderTime.print(countdownSeconds);
}

// ================== 组件回调 ==================

void sliderR_callback(int32_t value) {
    currentR = value;
    updateColor();
}

void sliderG_callback(int32_t value) {
    currentG = value;
    updateColor();
}

void sliderB_callback(int32_t value) {
    currentB = value;
    updateColor();
}

// 设置倒计时时长 (秒)
void sliderTime_callback(int32_t value) {
    countdownSeconds = value;
    BLINKER_LOG("Set Countdown Seconds: ", countdownSeconds);
}

// 开始/停止按钮回调
void btnStart_callback(const String & state) {
    if (state == BLINKER_CMD_ON) {
        // 用户按下开启
        if (countdownSeconds > 0) {
            currentMode = MODE_COUNTDOWN;
            countdownEndTime = millis() + (unsigned long)countdownSeconds * 1000;
            BLINKER_LOG("Countdown Started: ", countdownSeconds, " sec");
            TextTime.print(getRemainingTimeStr());
            BtnStart.print("on");
            BtnStart.text("停止倒计时");

            // 立即更新矩阵显示
            showTimeOnMatrix(countdownSeconds / 60, countdownSeconds % 60);
        } else {
            // 时间为0，无法开始
            BtnStart.print("off");
            BtnStart.text("请先设置时间");
            currentMode = MODE_NORMAL;
        }
    } else {
        // 用户按下停止
        currentMode = MODE_NORMAL;
        BLINKER_LOG("Countdown/Breathing Stopped");
        TextTime.print("已停止");
        BtnStart.print("off");
        BtnStart.text("开始倒计时");
        
        // 恢复之前的颜色
        updateColor();
    }
}

// ============================================

void setup() {
    Serial.begin(115200);
    BLINKER_DEBUG.stream(Serial);

    // 初始化 WS2812
    strip1.begin(); strip1.setBrightness(50); strip1.clear(); strip1.show();
    strip2.begin(); strip2.setBrightness(50); strip2.clear(); strip2.show();
    strip3.begin(); strip3.setBrightness(50); strip3.clear(); strip3.show();

    // 初始化 Blinker
    Blinker.begin();

    // 绑定回调函数
    Blinker.attachHeartbeat(heartbeat);
    SliderR.attach(sliderR_callback);
    SliderG.attach(sliderG_callback);
    SliderB.attach(sliderB_callback);
    SliderTime.attach(sliderTime_callback);
    BtnStart.attach(btnStart_callback);
    
    BLINKER_LOG("Blinker BLE Initialized");
    
    // 初始化默认颜色
    updateColor();
}

void loop() {
    Blinker.run();
    
    // 状态机逻辑
    if (currentMode == MODE_COUNTDOWN) {
        // 检查倒计时是否结束
        if (millis() >= countdownEndTime) {
            currentMode = MODE_BREATHING;
            BLINKER_LOG("Time's up! Entering Breathing Mode");
            TextTime.print("时间到!");
            BtnStart.text("停止呼吸灯");
        } else {
            // 没结束，每秒更新一次显示的剩余时间
            static unsigned long last_update_time = 0;
            if (millis() - last_update_time > 1000) {
                last_update_time = millis();
                TextTime.print(getRemainingTimeStr());
                
                // 更新矩阵显示
                long remaining = (long)(countdownEndTime - millis()) / 1000;
                if (remaining >= 0) {
                    showTimeOnMatrix(remaining / 60, remaining % 60);
                }
            }
        }
    } else if (currentMode == MODE_BREATHING) {
        // 呼吸灯特效
        static unsigned long last_breath_time = 0;
        // 每 20ms 更新一次亮度，实现平滑呼吸
        if (millis() - last_breath_time > 20) {
            last_breath_time = millis();
            
            // 使用正弦波生成呼吸因子 (0.1 ~ 1.0)
            float breath = (exp(sin(millis() / 2000.0 * PI)) - 0.36787944) * 0.425; 
            // 加上基底亮度，避免完全熄灭 (可选)
            if(breath < 0.1) breath = 0.1;
            
            // 计算当前呼吸亮度下的颜色
            uint8_t r = (uint8_t)(currentR * breath);
            uint8_t g = (uint8_t)(currentG * breath);
            uint8_t b = (uint8_t)(currentB * breath);
            
            uint32_t color = strip1.Color(r, g, b);
            setAllStripsColor(color);
        }
    }
}
