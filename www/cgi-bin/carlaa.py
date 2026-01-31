#!/usr/bin/env python3
import os
import sys
import urllib.parse

# Read POST data if available
post_data = ""
if os.environ.get("REQUEST_METHOD") == "POST":
    try:
        content_length = int(os.environ.get("CONTENT_LENGTH", 0))
        if content_length > 0:
            post_data = sys.stdin.read(content_length)
    except ValueError:
        pass

# Parse POST data
parsed_data = urllib.parse.parse_qs(post_data)
name = parsed_data.get("name", [""])[0]
email = parsed_data.get("email", [""])[0]
message = parsed_data.get("message", [""])[0]

print("Content-Type: text/html\r\n\r\n")

print("""<!DOCTYPE html>
<html lang=\"en\">
<head>
    <meta charset=\"UTF-8\">
    <title>CGI Response</title>
    <link rel=\"stylesheet\" href=\"/assets/css/styles.css\">
</head>
<body>
    <div class=\"container\">
        <div class=\"card\" style=\"margin-top: 2rem;\">
            <h1>CGI Response</h1>
""")

if name or email or message:
    print(f"""
            <div style="background-color: #d4edda; color: #155724; padding: 1rem; border-radius: 0.25rem; margin-bottom: 1rem;">
                <strong>Success!</strong> Form submitted successfully.
            </div>
            <h3>Received Data:</h3>
            <ul>
                <li><strong>Name:</strong> {name}</li>
                <li><strong>Email:</strong> {email}</li>
                <li><strong>Message:</strong> {message}</li>
            </ul>
    """)
else:
    print("""
            <p>This script prints selected CGI-related environment variables.</p>
            <ul>
    """)
    for key in [
        "REQUEST_METHOD",
        "REQUEST_URI",
        "QUERY_STRING",
        "CONTENT_TYPE",
        "CONTENT_LENGTH",
        "SERVER_PROTOCOL",
        "SERVER_NAME",
        "SERVER_PORT",
    ]:
        value = os.environ.get(key, "<not set>")
        print(f"<li><strong>{key}:</strong> {value}</li>")
    print("</ul>")

print("""
            <div class=\"mt-4\">
                <a href=\"/services.html\" class=\"btn btn-outline\" style="margin-left: 10px;">Back to Services</a>
            </div>
        </div>
    </div>
</body>
</html>
""")
