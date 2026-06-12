#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import sys
import uuid
import time
import urllib.parse

# 1. 强行告诉浏览器，我们要吐出的都是标准的 UTF-8 HTML 网页
print("Content-Type: text/html; charset=utf-8\r")

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))

database_dir = os.path.abspath(os.path.join(SCRIPT_DIR, "..", "database"))
success_page_path = os.path.abspath(os.path.join(SCRIPT_DIR, "..", "login", "success.html"))

def get_user_id_from_cookie():
    """Fetch the HTTP_COOKIE envp prepared by the parent process"""
    cookies = os.environ.get('HTTP_COOKIE', '')
    if cookies:
        for cookie in cookies.split(';'):
            parts = cookie.strip().split('=', 1)
            if len(parts) == 2 and parts[0] == "id":
                return parts[1]
    return None

user_id = get_user_id_from_cookie()

# ----------------- 🚀 主验证逻辑 -----------------
if user_id and os.path.exists(os.path.join(database_dir, f"{user_id}.txt")):
    with open(os.path.join(database_dir, f"{user_id}.txt"), "r") as f:
        lines = f.readlines()
    
    # 🚂 从密码本里安全取出三行数据
    username = lines[0].strip()
    password = lines[1].strip()
    chosen_color = lines[2].strip()

    # 🌟 打印空行，正式结束 HTTP Header 区域
    print("\r")

    # 🌟 只打印一次！使用正确的 username 和 password 变量，完美链式替换三个标签
    with open(success_page_path, "r", encoding="utf-8") as page:
        print(page.read()
              .replace("{{USERNAME}}", username)
              .replace("{{PASSWORD}}", password)
              .replace("{{COLOR}}", chosen_color.upper()))

# ----------------- 🛑 兜底逻辑（防止凭证失效时 C++ 卡死） -----------------
else:
    # 即使 Cookie 没匹配上，也必须打印空行并吐出点提示，绝对不能什么都不干就退出！
    print("\r")
    print("<h1>403 Forbidden - Invalid Session</h1>")