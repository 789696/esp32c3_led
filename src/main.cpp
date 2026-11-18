#define BLINKER_BLE

#include <Blinker.h>
#include <Adafruit_NeoPixel.h>

// ================= 配置区域 =================
// 注意：蓝牙模式下不需要密钥和WiFi信息
// 请在Blinker App中添加设备 -> 选择“蓝牙接入” -> 搜索连接本设备

// WS2812 灯板配置：16x16，单数据引脚 GPIO2
#define LED_PIN 2
#define MATRIX_WIDTH 16
#define MATRIX_HEIGHT 16
#define LED_COUNT (MATRIX_WIDTH * MATRIX_HEIGHT)

// 整板亮度 (0-255)，越大越亮，注意供电电流
#define LED_BRIGHTNESS 50

// 灯板方向微调：如果实际显示上下/左右相反，把对应项改为 1
#define MATRIX_FLIP_X 0
#define MATRIX_FLIP_Y 0

// ================= LED 矩阵映射与字体 =================
// 16x16 灯板为“逐行同向”排列（非蛇形）：
// 第 0 行从左到右为 0~15，第 1 行从左到右为 16~31，以此类推。
// 因此坐标 (x, y) 对应的灯珠序号为 y * 16 + x。

// 5x7 数字字体 (0-9)，每行低 5 位有效
const uint8_t DIGITS[10][7] = {
    {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}, // 0
    {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E}, // 1
    {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F}, // 2
    {0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E}, // 3
    {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}, // 4
    {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E}, // 5
    {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E}, // 6
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}, // 7
    {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}, // 8
    {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C}  // 9
};

// ===========================================

// 初始化 NeoPixel 对象（单引脚驱动整块 16x16 灯板）
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

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
bool lastBleConnected = false;      // 上一次的蓝牙连接状态

// ================= 矩阵显示辅助函数 =================

/**
 * @brief 将矩阵坐标转换为灯珠序号（逐行同向排列）
 */
inline int matrixToIndex(int x, int y) {
    if (MATRIX_FLIP_X) x = MATRIX_WIDTH - 1 - x;
    if (MATRIX_FLIP_Y) y = MATRIX_HEIGHT - 1 - y;
    return y * MATRIX_WIDTH + x;
}

/**
 * @brief 在矩阵指定坐标绘制像素
 */
void drawPixel(int x, int y, uint32_t color) {
    if (x < 0 || x >= MATRIX_WIDTH || y < 0 || y >= MATRIX_HEIGHT) return;
    strip.setPixelColor(matrixToIndex(x, y), color);
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
    for (int dy = 0; dy < 7; dy++) {
        for (int dx = 0; dx < 5; dx++) {
            // 检查字体位图 (从高位到低位: bit 4 ~ 0)
            if ((DIGITS[digit][dy] >> (4 - dx)) & 0x01) {
                drawPixel(x + dx, y + dy, color);
            }
        }
    }
}

/**
 * @brief 清除矩阵显示（仅清缓冲区，不立即刷新）
 */
void clearMatrix() {
    strip.clear();
}

/**
 * @brief 获取当前生效的 RGB（全黑时使用默认白色，避免什么都看不见）
 */
void getActiveRGB(uint8_t &r, uint8_t &g, uint8_t &b) {
    if (currentR == 0 && currentG == 0 && currentB == 0) {
        r = 50; g = 50; b = 50;
    } else {
        r = (uint8_t)currentR;
        g = (uint8_t)currentG;
        b = (uint8_t)currentB;
    }
}

/**
 * @brief 在矩阵上显示时间 (上排 MM，下排 SS)
 */
void showTimeOnMatrix(int minutes, int seconds) {
    clearMatrix();

    uint8_t r, g, b;
    getActiveRGB(r, g, b);
    uint32_t color = strip.Color(r, g, b);

    if (minutes < 0) minutes = 0;
    if (minutes > 99) minutes = 99;
    if (seconds < 0) seconds = 0;
    if (seconds > 99) seconds = 99;

    // 分钟 (上排)：两个 5x7 数字，水平居中
    drawDigit(2, 0, minutes / 10, color);
    drawDigit(9, 0, minutes % 10, color);

    // 秒数 (下排)
    drawDigit(2, 9, seconds / 10, color);
    drawDigit(9, 9, seconds % 10, color);

    strip.show();
}

// ==================================================

// 辅助函数：整板填充同一颜色
void fillAll(uint32_t color) {
    for (int i = 0; i < LED_COUNT; i++) {
        strip.setPixelColor(i, color);
    }
    strip.show();
}

// ================= 未连接蓝牙时的等待动画 =================
// 绿色雷达波：中心呼吸点 + 从中心向外扩散的光环，提示正在等待 App 连接
#define BLE_WAIT_ANIM_PERIOD_MS 1800 // 一轮扩散的时长
#define BLE_WAIT_ANIM_FRAME_MS 30    // 动画帧间隔

void showBleWaitingAnimation() {
    static unsigned long lastFrame = 0;
    unsigned long now = millis();
    if (now - lastFrame < BLE_WAIT_ANIM_FRAME_MS) return;
    lastFrame = now;

    // 0~1 的相位：光环半径随时间增大，同时逐渐变暗
    float phase = (float)(now % BLE_WAIT_ANIM_PERIOD_MS) / BLE_WAIT_ANIM_PERIOD_MS;
    float radius = phase * 11.0f;
    float fade = 1.0f - phase;

    strip.clear();
    for (int y = 0; y < MATRIX_HEIGHT; y++) {
        for (int x = 0; x < MATRIX_WIDTH; x++) {
            float dx = x - (MATRIX_WIDTH - 1) / 2.0f;
            float dy = y - (MATRIX_HEIGHT - 1) / 2.0f;
            float dist = sqrtf(dx * dx + dy * dy);

            uint8_t r = 0, g = 0, b = 0;

            // 中心呼吸点（2x2 像素）
            if (dist < 0.9f) {
                float pulse = 0.35f + 0.65f * (0.5f + 0.5f * sinf(now / 300.0f));
                g = (uint8_t)(80 + 175 * pulse);
            }

            // 向外扩散的光环
            float diff = fabsf(dist - radius);
            if (diff < 1.0f) {
                float k = (1.0f - diff) * fade;
                uint8_t ringG = (uint8_t)(255 * k);
                if (ringG > g) g = ringG;
            }

            if (g || b) strip.setPixelColor(matrixToIndex(x, y), strip.Color(r, g, b));
        }
    }
    strip.show();
}

/**
 * @brief 更新LED颜色
 * 将当前的RGB值应用到整块灯板 (仅在非呼吸模式下生效)
 */
void updateColor() {
    if (currentMode == MODE_BREATHING) return; // 呼吸模式下不响应滑块直接变色

    uint8_t r, g, b;
    getActiveRGB(r, g, b);
    fillAll(strip.Color(r, g, b));

    BLINKER_LOG("Color Updated: R=", currentR, ", G=", currentG, ", B=", currentB);
}

/**
 * @brief 计算剩余秒数（向上取整，避免刚开始就少显示 1 秒）
 */
long getRemainingSeconds() {
    long remainingMs = (long)(countdownEndTime - millis());
    if (remainingMs <= 0) return 0;
    return (remainingMs + 999) / 1000;
}

/**
 * @brief 格式化剩余时间
 */
String getRemainingTimeStr() {
    if (currentMode != MODE_COUNTDOWN) return "停止";

    long remaining = getRemainingSeconds();
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

    // 初始化 WS2812 灯板
    strip.begin();
    strip.setBrightness(LED_BRIGHTNESS);
    strip.clear();
    strip.show();

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

    bool bleConnected = Blinker.connected();

    // 空闲且未连接时播放等待动画；刚连上时恢复用户设置的颜色
    if (currentMode == MODE_NORMAL) {
        if (!bleConnected) {
            showBleWaitingAnimation();
        } else if (!lastBleConnected) {
            updateColor();
        }
    }
    lastBleConnected = bleConnected;

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
                long remaining = getRemainingSeconds();
                showTimeOnMatrix(remaining / 60, remaining % 60);
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
            if (breath < 0.1) breath = 0.1;

            // 计算当前呼吸亮度下的颜色
            uint8_t baseR, baseG, baseB;
            getActiveRGB(baseR, baseG, baseB);
            uint8_t r = (uint8_t)(baseR * breath);
            uint8_t g = (uint8_t)(baseG * breath);
            uint8_t b = (uint8_t)(baseB * breath);

            fillAll(strip.Color(r, g, b));
        }
    }
}
