#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import sys
import uuid
import time
import urllib.parse

print("Content-Type: text/html; charset=utf-8\r")

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))

def parse_request_body():
    """parsing request body (query string) input ex: username=cat&color=orange"""
    """output ex: {"username: ["cat"], "color": ["orange"]"} """
    try:
        content_length = int(os.environ.get('CONTENT_LENGTH', 0))
        if content_length == 0:
            return {}
        raw_body = sys.stdin.read(content_length)

        """ urllib.parse.parse_qs converts "username=cat" into {"username": ["cat"]}
         In HTTP, one key can have multiple values, so the value is always an Array.
         Example: {"username": ["cat", "dog"]} -> Index 0 is "cat", Index 1 is "dog"."""
        parsed_dict = urllib.parse.parse_qs(raw_body)

        """ clean_form is equivalent to std::map<std::string, std::string> in cpp"""
        clean_form = {}

        """iterate the hash map: key points to the value Array"""
        for key, value_array in parsed_dict.items():
            """Key points to the Array. We extract the 1st string at Index 0"""
            first_value = value_array[0]
            clean_form[key] = first_value 
        return clean_form

    except Exception:
        return {}

#Creating the database folder in ./www/database 
database_dir = os.path.abspath(os.path.join(SCRIPT_DIR, "..", "database"))

OFFICIAL_COLORS = ["bio-red", "cyber-purple", "hacker-green"]

form = parse_request_body()
name = form.get("username")
pwd = form.get("password")
chosen_color = form.get("color")

#------------------------the main logic here------------------------------
if (not chosen_color) or (chosen_color not in OFFICIAL_COLORS):
    chosen_color = "hacker-green"

# ----------------- Case 1: form submission or 1st Visit -----------------
"""curl -v -X POST -d "username=cat&password=123&color=bio-red" http://localhost:8080/login/login.py"""
"""curl -v -X POST -d "username=cat&password=123" http://localhost:8080/login/ (complete the cgi script filename inside the webserv program)"""
"""curl -v -X POST -d "username=cat&password=123" -c ./webserv_cookie.txt http://localhost:8080/login/login.py (creating the cookie file in user agent current folder)"""
"""curl -v GET http://localhost:8080/login/login.py"""
"""curl -v GET http://localhost:8080/login/ (complete the cgi script filename inside the webserv program)"""
if name and pwd: #form submission
    new_id = str(uuid.uuid4()) #Unique Universal Identifier generated (Session ID)
    expires = time.strftime("%a, %d-%b-%Y %H:%M:%S GMT", time.gmtime(time.time() + 3600)) #Session ID expired in 1h
    
    #Set-Cooke reponse header triggers the browser to create a local cookie file (client side)
    print(f"Set-Cookie: id={new_id}; Expires={expires}; Path=/\r") 
  
    print(f"Set-Cookie: lab_mutation={chosen_color}; Expires={expires}; Path=/\r")
    print("\r")

    #Create database folder if not exist
    if not os.path.exists(database_dir): 
        os.makedirs(database_dir)
    #Create a new file in this database folder, and name it with the session id, fill it out with username + password!
    with open(os.path.join(database_dir, f"{new_id}.txt"), "w") as f:
        f.write(f"{name}\n{pwd}\n{chosen_color}")

    #Extract the success.html template and replace usernane and password of this page
    success_page_path = os.path.join(SCRIPT_DIR, "success.html")
    with open(success_page_path, "r", encoding="utf-8") as page:
        print(page.read().replace("{{USERNAME}}", name).replace("{{PASSWORD}}", pwd).replace("{{COLOR}}", chosen_color.upper()))
else: # 1st visit
    print("\r")
    login_page_path = os.path.join(SCRIPT_DIR, "login_index.html")
    with open(login_page_path, "r", encoding="utf-8") as page:
        print(page.read())