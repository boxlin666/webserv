#!/usr/bin/env python3
import os
import sys
from http import cookies

# 1. 尝试从环境变量抓取客户端带过来的 Cookie
cookie = cookies.SimpleCookie(os.environ.get("HTTP_COOKIE", ""))
session_id = cookie.get("id").value if cookie.get("id") else None

# 2. 如果抓到了钥匙，直接去后方大本营把档案袋【物理毁灭】
if session_id:
    session_file = f"./www/login/database/{session_id}.txt"
    if os.path.exists(session_file):
        os.remove(session_file) # 👈 斩草除根！

# 3. 双管齐下：下发 Header，命令客户端把钥匙寿命设为 0（立刻暴毙）
print("Status: 302 Found") # 重定向回首页
print("Set-Cookie: id=deleted; path=/; max-age=0") # 👈 前线缴枪！
print("Location: /index.html") # 把用户一脚踢回大首页
print("\r\n")