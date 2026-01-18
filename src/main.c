/* main.c */

#include <stdio.h>

// 声明 streamer 中的测试函数
void streamer_init_test();

int main() {
    puts("Hello World from C Language!");
    puts("This runs on RV1126 with standard libraries.");
    
    // 测试 FFmpeg 集成
    streamer_init_test();
    
    return 0;
}