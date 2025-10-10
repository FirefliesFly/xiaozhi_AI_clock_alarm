#include <cstring>
#include "display/lcd_display.h"
#include <esp_log.h>
#include "mmap_generate_emoji.h"
#include "emoji_display.h"
#include "assets/lang_config.h"

#include <esp_lcd_panel_io.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/event_groups.h>

#include "result_test.h"

static const char *TAG = "emoji";

namespace anim {

bool EmojiPlayer::OnFlushIoReady(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    auto* disp_drv = static_cast<anim_player_handle_t*>(user_ctx);
    anim_player_flush_ready(disp_drv);
    return true;
}

static const uint8_t* test_array[]={
    muka0_column,
    muka1_column,
    muka2_column,
    muka3_column,
    muka4_column,
    muka5_column,
    muka6_column,
    muka7_column,
    muka8_column,
    muka9_column,
    muka10_column,
    muka11_column,
    muka12_column,
    muka13_column,
    muka14_column,
    muka15_column,
    muka16_column,
    muka17_column,
    muka18_column,
    muka19_column,
    muka20_column,
    muka21_column,
    muka22_column,
    muka23_column,
    muka24_column,
    muka25_column,
    muka26_column,
    muka27_column,
    muka28_column,
    muka29_column,
    muka30_column,
    muka31_column,
    muka32_column,
    muka33_column,
    muka34_column,
    muka35_column,
    muka36_column,
    muka37_column,
    muka38_column,
    muka39_column,
    muka40_column,
    muka41_column,
    muka42_column,
    muka43_column,
    muka44_column,
    muka45_column,
    muka46_column,
    muka47_column,
    muka48_column,
    muka49_column,
    muka50_column,
    muka51_column,
    muka52_column,
    muka53_column,
    muka54_column,
    muka55_column,
    muka56_column,
    muka57_column,
    muka58_column,
    muka59_column,
    muka60_column,
    muka61_column,
    muka62_column,
    muka63_column,
    muka64_column,
    muka65_column,
    muka66_column,
    muka67_column,
    muka68_column,
    muka69_column,
    muka70_column,
    muka71_column,
    muka72_column,
    muka73_column,
    muka74_column,
};

void EmojiPlayer::OnFlush(anim_player_handle_t handle, int x_start, int y_start, int x_end, int y_end, const void *color_data)
{
    /**
     * 获取anim_player这个task操作的panel句柄（该句柄指向一个实际驱动实现，所以可以通过这个实际的驱动实现，将动画数据输出到屏幕中）
     */
    auto* panel = static_cast<esp_lcd_panel_handle_t>(anim_player_get_user_data(handle));
    // ESP_LOGI(TAG, "HELLO!! onflush x_start=%d y_start=%d x_end=%d y_end=%d", x_start, y_start, x_end, y_end);
    // esp_lcd_panel_draw_bitmap(panel, x_start, y_start, x_end, y_end, color_data);

    static uint8_t test_cnt = 0;

    esp_lcd_panel_draw_bitmap(panel, 0, 0, 127, 63, test_array[test_cnt++]);
    if(test_cnt > 75)
    {
        test_cnt = 0;
    }
}

EmojiPlayer::EmojiPlayer(esp_lcd_panel_handle_t panel, esp_lcd_panel_io_handle_t panel_io)
{
    const mmap_assets_config_t assets_cfg = {
        .partition_label = "assets_A",
        .max_files = MMAP_EMOJI_FILES,
        .checksum = MMAP_EMOJI_CHECKSUM,
        .flags = {.mmap_enable = true, .full_check = true}
    };

    mmap_assets_new(&assets_cfg, &assets_handle_);

    anim_player_config_t player_cfg = {
        .flush_cb = OnFlush,//将这个替换掉
        .update_cb = NULL,
        .user_data = panel,
        .flags = {.swap = false},
        .task = ANIM_PLAYER_INIT_CONFIG()
    };

    /**
     * 由于ESPC3 OLED的驱动前面已经注册实现了，这里就只需要将anim_player任务注册好，
     * 并且通过player_cfg来关联上panel这个驱动实现句柄就可以了
     */

    player_handle_ = anim_player_init(&player_cfg);

    /**
     * 如前面所述，这里取消掉驱动相关的实现，因为本级函数执行之前，OLED的驱动实例已经注册好了
     */
    /*
    const esp_lcd_panel_io_callbacks_t cbs = {
        .on_color_trans_done = OnFlushIoReady,
    };

    esp_lcd_panel_io_register_event_callbacks(panel_io, &cbs, player_handle_);
    */

    StartPlayer(MMAP_EMOJI_CONNECTING_AAF, true, 15);
}

EmojiPlayer::~EmojiPlayer()
{
    if (player_handle_) {
        anim_player_update(player_handle_, PLAYER_ACTION_STOP);
        anim_player_deinit(player_handle_);
        player_handle_ = nullptr;
    }

    if (assets_handle_) {
        mmap_assets_del(assets_handle_);
        assets_handle_ = NULL;
    }
}

void EmojiPlayer::StartPlayer(int aaf, bool repeat, int fps)
{
    if (player_handle_) {
        uint32_t start, end;
        const void *src_data;
        size_t src_len;

        src_data = mmap_assets_get_mem(assets_handle_, aaf);
        src_len = mmap_assets_get_size(assets_handle_, aaf);

        anim_player_set_src_data(player_handle_, src_data, src_len);
        anim_player_get_segment(player_handle_, &start, &end);
        if(MMAP_EMOJI_WAKE_AAF == aaf){
            start = 7;
        }

        fps = 1;
        anim_player_set_segment(player_handle_, start, end, fps, true);
        anim_player_update(player_handle_, PLAYER_ACTION_START);
    }
}

void EmojiPlayer::StopPlayer()
{
    if (player_handle_) {
        anim_player_update(player_handle_, PLAYER_ACTION_STOP);
    }
}

EmojiWidget::EmojiWidget(esp_lcd_panel_handle_t panel, esp_lcd_panel_io_handle_t panel_io)
{
    ESP_LOGI(TAG, "HELLO!!! init EmojiPlayer, panel: %p, panel_io: %p", panel, panel_io);

    InitializePlayer(panel, panel_io);
}

EmojiWidget::~EmojiWidget()
{

}

void EmojiWidget::SetEmotion(const char* emotion)
{
    if (!player_) {
        return;
    }

    using Param = std::tuple<int, bool, int>;
    static const std::unordered_map<std::string, Param> emotion_map = {
        {"happy",       {MMAP_EMOJI_HAPPY_LOOP_AAF, true, 25}},
        {"laughing",    {MMAP_EMOJI_HAPPY_LOOP_AAF, true, 25}},
        {"funny",       {MMAP_EMOJI_HAPPY_LOOP_AAF, true, 25}},
        {"loving",      {MMAP_EMOJI_HAPPY_LOOP_AAF, true, 25}},
        {"embarrassed", {MMAP_EMOJI_HAPPY_LOOP_AAF, true, 25}},
        {"confident",   {MMAP_EMOJI_HAPPY_LOOP_AAF, true, 25}},
        {"delicious",   {MMAP_EMOJI_HAPPY_LOOP_AAF, true, 25}},
        {"sad",         {MMAP_EMOJI_SAD_LOOP_AAF,   true, 25}},
        {"crying",      {MMAP_EMOJI_SAD_LOOP_AAF,   true, 25}},
        {"sleepy",      {MMAP_EMOJI_SAD_LOOP_AAF,   true, 25}},
        {"silly",       {MMAP_EMOJI_SAD_LOOP_AAF,   true, 25}},
        {"angry",       {MMAP_EMOJI_ANGER_LOOP_AAF, true, 25}},
        {"surprised",   {MMAP_EMOJI_PANIC_LOOP_AAF, true, 25}},
        {"shocked",     {MMAP_EMOJI_PANIC_LOOP_AAF, true, 25}},
        {"thinking",    {MMAP_EMOJI_HAPPY_LOOP_AAF, true, 25}},
        {"winking",     {MMAP_EMOJI_BLINK_QUICK_AAF, true, 5}},
        {"relaxed",     {MMAP_EMOJI_SCORN_LOOP_AAF, true, 25}},
        {"confused",    {MMAP_EMOJI_SCORN_LOOP_AAF, true, 25}},
    };

    auto it = emotion_map.find(emotion);
    if (it != emotion_map.end()) {
        const auto& [aaf, repeat, fps] = it->second;
        player_->StartPlayer(aaf, repeat, fps);
    } else if (strcmp(emotion, "neutral") == 0) {
    }
}

void EmojiWidget::SetStatus(const char* status)
{
    ESP_LOGI(TAG, "HELLO!!! Emoji set status");
    if (player_) {
        ESP_LOGI(TAG, "HELLO!!! status = %s", status);

        // if (strcmp(status, Lang::Strings::LISTENING) == 0) {
        //    player_->StartPlayer(MMAP_EMOJI_ASKING_AAF, true, 15);
        // } else if (strcmp(status, Lang::Strings::STANDBY) == 0) {
        //     player_->StartPlayer(MMAP_EMOJI_WAKE_AAF, true, 15);
        // }
        player_->StartPlayer(MMAP_EMOJI_BLINK_ONCE_AAF, true, 15);
    }
}

void EmojiWidget::InitializePlayer(esp_lcd_panel_handle_t panel, esp_lcd_panel_io_handle_t panel_io)
{
    player_ = std::make_unique<EmojiPlayer>(panel, panel_io);
}

bool EmojiWidget::Lock(int timeout_ms)
{
    return true;
}

void EmojiWidget::Unlock()
{
}

} // namespace anim
