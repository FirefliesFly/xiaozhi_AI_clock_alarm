#include <cstring>
#include "display/lcd_display.h"
#include <esp_log.h>
// #include "mmap_generate_emoji.h"
#include "mmap_generate_assets_A.h"
#include "emoji_display.h"
#include "assets/lang_config.h"

#include <esp_lcd_panel_io.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/event_groups.h>

// #include "result_test.h"

static const char *TAG = "emoji";

namespace anim {

bool EmojiPlayer::OnFlushIoReady(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    auto* disp_drv = static_cast<anim_player_handle_t*>(user_ctx);
    anim_player_flush_ready(disp_drv);
    return true;
}

void EmojiPlayer::OnFlush(anim_player_handle_t handle, int x_start, int y_start, int x_end, int y_end, const void *color_data)
{
    /**
     * 获取anim_player这个task操作的panel句柄（该句柄指向一个实际驱动实现，所以可以通过这个实际的驱动实现，将动画数据输出到屏幕中）
     */
    auto* panel = static_cast<esp_lcd_panel_handle_t>(anim_player_get_user_data(handle));
    // ESP_LOG_BUFFER_HEXDUMP("HELLO!! flush=", color_data, 128*8, ESP_LOG_INFO);
 
    esp_lcd_panel_draw_bitmap(panel, x_start, y_start, x_end-1, y_end-1, color_data);
}

#include "esp_partition.h"

EmojiPlayer::EmojiPlayer(esp_lcd_panel_handle_t panel, esp_lcd_panel_io_handle_t panel_io)
{
    const mmap_assets_config_t assets_cfg = {
        .partition_label = "assets_A",
        /**
         * 这里的MMAP_ASSETS_A_FILES和MMAP_ASSETS_A_CHECKSUM是mmap_generate_assets_A.h中定义的，
         * 运行mmap_generate_assets_A.py脚本生成mmap_generate_assets_A.h文件，
         * 这个脚本会根据assets_A分区中的文件数量和文件内容进行计算，并生成这两个宏定义
         * 
         * 注意：max_files必须不能超过预存在assets_A分区里的文件数量，如果超过，那么在mmap_assets_new()读取注册的时候，mmap协议读取大概率出错，因为越界错误访问
         * 
         */
        .max_files = MMAP_ASSETS_A_FILES,
        .checksum = MMAP_ASSETS_A_CHECKSUM,
        .flags = {.mmap_enable = true, .full_check = true}
    };

    mmap_assets_new(&assets_cfg, &assets_handle_);

    // // 打印文件数量和信息
    // ESP_LOGI(TAG, "Total files in assets_A: %d", mmap_assets_get_stored_files(assets_handle_));

    // // 遍历所有文件
    // for (int i = 0; i < mmap_assets_get_stored_files(assets_handle_); i++) {
    //     const char* filename = mmap_assets_get_name(assets_handle_, i);
    //     size_t filesize = mmap_assets_get_size(assets_handle_, i);
    //     ESP_LOGI(TAG, "File %d: %s (size: %d bytes)", i, filename, filesize);
    // }

    // const esp_partition_t* partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "assets_A");
    // ESP_LOGI(TAG, "HELLO!!! ssdd");
    // if (partition != NULL) {
    //     ESP_LOGI(TAG, "HELLO!!! partition->size=%d", partition->size);
    //     uint8_t* buffer = (uint8_t*)malloc(12000);
    //     esp_partition_read(partition, 0, buffer, 12000);
    //     ESP_LOG_BUFFER_HEX(TAG, buffer, 12000);
    //     free(buffer);

    //     ESP_LOGI(TAG, "HELLO!!! mmset");
    //     uint8_t *my_test = (uint8_t *)mmap_assets_get_mem(assets_handle_, 0);
    //     size_t filesize = mmap_assets_get_size(assets_handle_, 0);
    //     ESP_LOG_BUFFER_HEX(TAG, my_test, filesize);
    //     // 现在buffer中包含了整个分区的数据，可以将其保存到文件或通过串口发送
    // }


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

    /**
     * 取消开机初始化阶段的动画播放，以防止覆盖掉初始化阶段的其它界面提示，对用户不友好
     */
    // StartPlayer(MMAP_EMOJI_CONNECTING_AAF, true, 15);
    StopPlayer();
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

// void EmojiPlayer::StartPlayer(int aaf, bool repeat, int fps)
// {
//     if (player_handle_) {
//         /**
//          * @param start 告诉anim task应该从一帧画面里的的第几行像素开始播放
//          * @param end 告诉anim task应该播放到一帧画面里的第几行作为结束
//          */
//         uint32_t start, end;
//         const void *src_data;
//         size_t src_len;
//         /**
//          * 因为使用esp32的anim组件，需要从anim任务外边告知anim task应该从哪里获取到动画的资源，
//          * 也即需要告诉anim task动画的内容所在的位置，
//          * 这里就是载入了对应的内存位置，也即aaf所在的内存位置，通过anim_player_set_src_data()来告知anim task
//          * 然后调用anim_player_update()通知anim task开始播放动画
//          * 
//          */
//         src_data = mmap_assets_get_mem(assets_handle_, aaf);
//         src_len = mmap_assets_get_size(assets_handle_, aaf);

//         anim_player_set_src_data(player_handle_, src_data, src_len);
//         anim_player_get_segment(player_handle_, &start, &end);
//         if(MMAP_EMOJI_WAKE_AAF == aaf){
//             start = 7;
//         }

//         fps = 1;
//         anim_player_set_segment(player_handle_, start, end, fps, true);
//         anim_player_update(player_handle_, PLAYER_ACTION_START);
//     }
// }

void EmojiPlayer::StartPlayer(int aaf, bool repeat, int fps)
{
    if (player_handle_) {

        // static uint8_t my_cnt = 0;
        // aaf = my_cnt % (MMAP_ASSETS_A_SLEEPY_3_BIN + 1);
        // my_cnt++;
        // if(my_cnt == (MMAP_ASSETS_A_SLEEPY_3_BIN + 1))
        // {
        //     my_cnt = 0;
        // }
        /**
         * @param start 告诉anim task应该从一帧画面里的的第几行像素开始播放
         * @param end 告诉anim task应该播放到一帧画面里的第几行作为结束
         */
        uint32_t start, end;
        const void *src_data;
        size_t src_len;
        /**
         * 因为使用esp32的anim组件，需要从anim任务外边告知anim task应该从哪里获取到动画的资源，
         * 也即需要告诉anim task动画的内容所在的位置，
         * 这里就是载入了对应的内存位置，也即aaf所在的内存位置，通过anim_player_set_src_data()来告知anim task
         * 然后调用anim_player_update()通知anim task开始播放动画
         * 
         */
        src_data = mmap_assets_get_mem(assets_handle_, aaf);
        src_len = mmap_assets_get_size(assets_handle_, aaf);

        anim_player_set_src_data(player_handle_, src_data, src_len);
        anim_player_get_segment(player_handle_, &start, &end);

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
#if 0
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
#else
    static const std::unordered_map<std::string, Param> emotion_map = {
        // 中性/平静类表情 -> staticstate
        {"relaxed",     {MMAP_ASSETS_A_RELAXED_BIN, true, 50}},
        {"sleepy",      {MMAP_ASSETS_A_EMBARRASSED_BIN,   true, 50}},
        // {"neutral", &staticstate},

        // 积极/开心类表情 -> happy
        {"happy",       {MMAP_ASSETS_A_HAPPY_2_BIN, true, 50}},//表情是旋转过来的样子，调皮样
        {"laughing",    {MMAP_ASSETS_A_LAUGH_BIN, true, 15}},//就是盯着屏幕前，然后眨眼，然后眼皮低垂
        {"funny",       {MMAP_ASSETS_A_ENJOY_BIN, true, 15}},//就是盯着屏幕前，然后眨眼，然后眼皮低垂
        {"loving",      {MMAP_ASSETS_A_LOVE_BIN, true, 25}},//
        {"confident",   {MMAP_ASSETS_A_PROUD_BIN, true, 25}},
        {"delicious",   {MMAP_ASSETS_A_ENJOY_BIN, true, 50}},
        {"silly",       {MMAP_ASSETS_A_SLEEPY_3_BIN,   true, 25}},//MMAP_ASSETS_A_SLEEPY_3_BIN是飘雪 转头吃血的顽皮
        {"winking",     {MMAP_ASSETS_A_ANGRY_BIN, true, 50}},
        {"cool",        {MMAP_ASSETS_A_CONTENT_BIN,  true, 25}},//就是吃包子的表情，满足
        // {"kissy", &happy},

        // 悲伤类表情 -> sad
        {"sad",         {MMAP_ASSETS_A_SLEEPY_3_BIN,   true, 25}},
        {"crying",      {MMAP_ASSETS_A_SLEEPY_3_BIN,   true, 25}},
        
        // 愤怒类表情 -> anger
        // {"angry",       {, true, 50}},

        // 惊讶类表情 -> scare
        {"surprised",   {MMAP_ASSETS_A_HAPPY_BIN, true, 25}},
        {"shocked",     {MMAP_ASSETS_A_FRUSTRATED_BIN, true, 25}},

        // 思考/困惑类表情 -> buxue
        {"embarrassed", {MMAP_ASSETS_A_EMBARRASSED_BIN, true, 25}},
        {"thinking",    {MMAP_ASSETS_A_CONFUSED_2_BIN, true, 25}},
        {"confused",    {MMAP_ASSETS_A_CONFUSED_2_BIN, true, 25}},
    };
#endif
    auto it = emotion_map.find(emotion);
    if (it != emotion_map.end()) {
        const auto& [aaf, repeat, fps] = it->second;
        ESP_LOGI(TAG, "HELLO!!! Emoji set emotion: %d", aaf);
        player_->StartPlayer(aaf, repeat, fps);
    } else if (strcmp(emotion, "neutral") == 0) {
    }
}

void EmojiWidget::StopPlayer()
{
    if (player_) {
        player_->StopPlayer();
    }
    else
    {
        ESP_LOGE(TAG, "NO Emoji stop player driver");
    }
}

void EmojiWidget::SetStatus(const char* status)
{
    ESP_LOGI(TAG, "HELLO!!! Emoji set status");
    if (player_) {
        ESP_LOGI(TAG, "HELLO!!! status = %s", status);

        if (strcmp(status, Lang::Strings::LISTENING) == 0) {
           player_->StartPlayer(MMAP_ASSETS_A_INTRO_BIN, true, 15);
        } else if (strcmp(status, Lang::Strings::STANDBY) == 0) {
            player_->StartPlayer(MMAP_ASSETS_A_CONTENT_BIN, true, 15);//wink
        }
        else if (strcmp(status, Lang::Strings::CONNECTING) == 0)
        {
            player_->StartPlayer(MMAP_ASSETS_A_SLEEPY_BIN, true, 15);//wink
        }
        
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
