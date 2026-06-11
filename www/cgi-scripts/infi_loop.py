#!/usr/bin/env python3
import sys
import time

def main():
    # 按照 HTTP/CGI 规范，先打印标准的头部
    print("Content-Type: text/plain\r\n\r\n", end="")
    sys.stdout.flush()

    try:
        while True:
            # 优化点 1: flush=True 强刷缓冲区，确保每一次输出都立刻触碰底层管道
            print("Hello world!", flush=True)
            
            # 稍微加一点极短的延迟（比如 0.01 秒），防止 CPU 瞬间飙到 100% 导致电脑发烫
            time.sleep(0.1)
            
    except BrokenPipeError:
        # 优化点 2: 捕获管道破裂异常。当客户端（curl）断开时，Python 会走进这里
        # 注意：此时标准输出已经关了，只能往 stderr 打印调试日志
        print("[CGI Debug] Client disconnected. Broken pipe caught, exiting gracefully...", file=sys.stderr)
        sys.exit(0)
        
    except KeyboardInterrupt:
        # 顺便捕获一下 Ctrl+C
        sys.exit(0)

if __name__ == "__main__":
    main()