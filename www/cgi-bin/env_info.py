#!/usr/bin/env python3
import os

print("Content-Type: text/html\r\n\r\n")

print("""<!DOCTYPE html>
<html lang=\"en\">
<head>
    <meta charset=\"UTF-8\">
    <title>CGI Environment Info</title>
    <link rel=\"stylesheet\" href=\"/assets/css/styles.css\">
</head>
<body>
    <div class=\"container\">
        <div class=\"card\" style=\"margin-top: 2rem;\">
            <h1>CGI Environment Variables</h1>
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

print("""
            </ul>
            <div class=\"mt-4\">
                <a href=\"/services.html\" class=\"btn btn-outline\">Back to Services</a>
            </div>
        </div>
    </div>
</body>
</html>
""")
