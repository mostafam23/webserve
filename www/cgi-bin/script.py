#!/usr/bin/env python3
import os
import sys

# CGI header
print("Content-Type: text/html\r\n\r\n")

print("""<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <title>CGI Test Result</title>
    <link rel="stylesheet" href="/assets/css/styles.css">
</head>
<body>
    <div class="container">
        <div class="card" style="margin-top: 2rem;">
            <h1>CGI Test Result</h1>
            <p>Here is the data received by the CGI script.</p>
            
            <h3>Environment Variables</h3>
            <ul>
""")

for key in [
    "REQUEST_METHOD",
    "CONTENT_LENGTH",
    "CONTENT_TYPE",
    "QUERY_STRING"
]:
    value = os.environ.get(key, "")
    print(f"<li><strong>{key}:</strong> {value}</li>")

print("""
            </ul>
            
            <h3>Request Body</h3>
            <pre style="background: #f4f4f4; padding: 1rem; border-radius: 4px;">""")

# Read stdin if content-length is set
try:
    content_length = int(os.environ.get("CONTENT_LENGTH", 0))
    if content_length > 0:
        body = sys.stdin.read(content_length)
        print(body)
    else:
        print("(No body content)")
except Exception as e:
    print(f"Error reading body: {e}")

print("""</pre>
            <div class="mt-4">
                <a href="/services.html" class="btn btn-outline">Back to Services</a>
            </div>
        </div>
    </div>
</body>
</html>
""")