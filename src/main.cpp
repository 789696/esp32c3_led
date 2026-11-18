#define BLINKER_BLE

#include <Blinker.h>
#include <FastLED.h>

// ================= 配置区域 =================
// 注意：蓝牙模式下不需要密钥和WiFi信息
// 请在Blinker App中添加设备 -> 选择“蓝牙接入” -> 搜索连接本设备

// WS2812 灯板配置：16x16，单数据引脚 GPIO2
#define LED_PIN 2
#define MATRIX_WIDTH 16
#define MATRIX_HEIGHT 16
#define LED_COUNT (MATRIX_WIDTH * MATRIX_HEIGHT)

// 整板亮度 (0-255)，越大越亮，注意供电电流
// 设为 255 保留完整的 8 位灰度，低亮度颜色不会被截断
#define LED_BRIGHTNESS 255

// 灯板方向微调：如果实际显示上下/左右相反，把对应项改为 1
#define MATRIX_FLIP_X 0
#define MATRIX_FLIP_Y 0

// ================= LED 矩阵映射与字体 =================
// 16x16 灯板为“逐行同向”排列（非蛇形）：
// 第 0 行从左到右为 0~15，第 1 行从左到右为 16~31，以此类推。
// 因此坐标 (x, y) 对应的灯珠序号为 y * 16 + x。

// 5x9 数字字体 (0-9)，用于放大的分钟显示，每行低 5 位有效
const uint8_t DIGITS_LARGE[10][9] = {
    {0x0E, 0x11, 0x11, 0x13, 0x15, 0x19, 0x11, 0x11, 0x0E}, // 0
    {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E}, // 1
    {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x10, 0x10, 0x1F}, // 2
    {0x1F, 0x01, 0x02, 0x04, 0x02, 0x01, 0x01, 0x11, 0x0E}, // 3
    {0x02, 0x06, 0x0A, 0x12, 0x12, 0x1F, 0x02, 0x02, 0x02}, // 4
    {0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x01, 0x11, 0x0E}, // 5
    {0x06, 0x08, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x11, 0x0E}, // 6
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08, 0x08, 0x08}, // 7
    {0x0E, 0x11, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x11, 0x0E}, // 8
    {0x0E, 0x11, 0x11, 0x11, 0x0F, 0x01, 0x01, 0x02, 0x0C}  // 9
};

// 3x5 数字字体 (0-9)，用于缩小后的秒数显示，每行低 3 位有效
const uint8_t DIGITS_SMALL[10][5] = {
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

// 灯板颜色数据（FastLED 在 show() 时通过 RMT 发送到 GPIO2）
CRGB leds[LED_COUNT];

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
void drawPixel(int x, int y, CRGB color) {
    if (x < 0 || x >= MATRIX_WIDTH || y < 0 || y >= MATRIX_HEIGHT) return;
    leds[matrixToIndex(x, y)] = color;
}

/**
 * @brief 绘制单个数字
 * @param x 起始X坐标
 * @param y 起始Y坐标
 * @param digit 要显示的数字 (0-9)
 * @param color 颜色
 */
void drawDigitLarge(int x, int y, int digit, CRGB color) {
    if (digit < 0 || digit > 9) return;
    for (int dy = 0; dy < 9; dy++) {
        for (int dx = 0; dx < 5; dx++) {
            // 检查字体位图 (从高位到低位: bit 4 ~ 0)
            if ((DIGITS_LARGE[digit][dy] >> (4 - dx)) & 0x01) {
                drawPixel(x + dx, y + dy, color);
            }
        }
    }
}

void drawDigitSmall(int x, int y, int digit, CRGB color) {
    if (digit < 0 || digit > 9) return;
    for (int dy = 0; dy < 5; dy++) {
        for (int dx = 0; dx < 3; dx++) {
            // 检查字体位图 (从高位到低位: bit 2 ~ 0)
            if ((DIGITS_SMALL[digit][dy] >> (2 - dx)) & 0x01) {
                drawPixel(x + dx, y + dy, color);
            }
        }
    }
}

/**
 * @brief 清除矩阵显示（仅清缓冲区，不立即刷新）
 */
void clearMatrix() {
    FastLED.clear(); // 仅清缓冲区，不立即刷新
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
    CRGB color(r, g, b);

    if (minutes < 0) minutes = 0;
    if (minutes > 99) minutes = 99;
    if (seconds < 0) seconds = 0;
    if (seconds > 99) seconds = 99;

    // 分钟 (上排)：两个 5x9 大字，占满 0~8 行
    drawDigitLarge(2, 0, minutes / 10, color);
    drawDigitLarge(9, 0, minutes % 10, color);

    // 秒数 (下排)：两个 3x5 小字，占 11~15 行
    drawDigitSmall(3, 11, seconds / 10, color);
    drawDigitSmall(10, 11, seconds % 10, color);

    FastLED.show();
}

// ==================================================

// 辅助函数：整板填充同一颜色
void fillAll(CRGB color) {
    fill_solid(leds, LED_COUNT, color);
    FastLED.show();
}

// ================= 呼吸灯参数 =================
#define BREATH_PERIOD_MS 4000 // 一次完整呼吸的时长
#define BREATH_FRAME_MS 20    // 50fps，256 灯珠一帧约 7.7ms
#define BREATH_MIN_PEAK 32    // 颜色太暗时，呼吸期间按比例抬高整体峰值，保证各通道都看得出变化

// ================= 未连接蓝牙时的等待动画 =================
// 绿色雷达波：中心呼吸点 + 从中心向外扩散的光环，提示正在等待 App 连接
#define BLE_WAIT_ANIM_PERIOD_MS 1800 // 一轮扩散的时长
#define BLE_WAIT_ANIM_FRAME_MS 50    // 动画帧间隔（256 灯珠一帧约 7.7ms，留足 BLE 时间）
#define BLE_WAIT_ANIM_LEVEL 20       // 雷达动画自身亮度 (0-255)，与滑块的全局亮度独立

void showBleWaitingAnimation() {
    static unsigned long lastFrame = 0;
    unsigned long now = millis();
    if (now - lastFrame < BLE_WAIT_ANIM_FRAME_MS) return;
    lastFrame = now;

    // 0~1 的相位：光环半径随时间增大，同时逐渐变暗
    float phase = (float)(now % BLE_WAIT_ANIM_PERIOD_MS) / BLE_WAIT_ANIM_PERIOD_MS;
    float radius = phase * 11.0f;
    float fade = 1.0f - phase;

    FastLED.clear();
    for (int y = 0; y < MATRIX_HEIGHT; y++) {
        for (int x = 0; x < MATRIX_WIDTH; x++) {
            float dx = x - (MATRIX_WIDTH - 1) / 2.0f;
            float dy = y - (MATRIX_HEIGHT - 1) / 2.0f;
            float dist = sqrtf(dx * dx + dy * dy);

            uint8_t r = 0, g = 0, b = 0;

            // 中心呼吸点（2x2 像素）
            if (dist < 0.9f) {
                float pulse = 0.35f + 0.65f * (0.5f + 0.5f * sinf(now / 300.0f));
                g = (uint8_t)((80 + 175 * pulse) * BLE_WAIT_ANIM_LEVEL / 255);
            }

            // 向外扩散的光环
            float diff = fabsf(dist - radius);
            if (diff < 1.0f) {
                float k = (1.0f - diff) * fade;
                uint8_t ringG = (uint8_t)(k * BLE_WAIT_ANIM_LEVEL);
                if (ringG > g) g = ringG;
            }

            if (g || b) leds[matrixToIndex(x, y)] = CRGB(r, g, b);
        }
    }
    FastLED.show();
}

/**
 * @brief 更新LED颜色
 * 将当前的RGB值应用到整块灯板 (仅在非呼吸模式下生效)
 */
void updateColor() {
    if (currentMode == MODE_BREATHING) return; // 呼吸模式下不响应滑块直接变色

    uint8_t r, g, b;
    getActiveRGB(r, g, b);
    fillAll(CRGB(r, g, b));

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

    // 初始化 WS2812 灯板（FastLED 的 ESP32 RMT 驱动比 Adafruit 旧驱动更稳定）
    FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, LED_COUNT);
    FastLED.setBrightness(LED_BRIGHTNESS);
    FastLED.clear();
    FastLED.show();

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
            BLINKER_LOG("Breathing RGB: R=", currentR, ", G=", currentG, ", B=", currentB);
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
        // 呼吸灯特效：余弦曲线 0→1→0，起止都没有突变
        static unsigned long last_breath_time = 0;
        unsigned long now = millis();
        if (now - last_breath_time >= BREATH_FRAME_MS) {
            last_breath_time = now;

            float phase = (float)(now % BREATH_PERIOD_MS) / BREATH_PERIOD_MS;
            float breath = 0.5f - 0.5f * cosf(phase * 2.0f * PI);

            // 使用 FastLED 的 video 缩放，低亮度段过渡更细腻
            uint8_t baseR, baseG, baseB;
            getActiveRGB(baseR, baseG, baseB);

            // 当前颜色太暗时按比例抬高峰值，避免绿色等通道变化看不出来
            uint8_t peak = baseR;
            if (baseG > peak) peak = baseG;
            if (baseB > peak) peak = baseB;
            if (peak > 0 && peak < BREATH_MIN_PEAK) {
                uint16_t scale = (uint16_t)BREATH_MIN_PEAK * 256u / peak;
                baseR = (uint8_t)((baseR * scale) >> 8);
                baseG = (uint8_t)((baseG * scale) >> 8);
                baseB = (uint8_t)((baseB * scale) >> 8);
            }

            CRGB c(baseR, baseG, baseB);
            c.nscale8_video((uint8_t)(breath * 255.0f + 0.5f));

            fillAll(c);
        }
    }
}
