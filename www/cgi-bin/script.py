#!/usr/bin/env python3
import cgi
import cgitb
import os

cgitb.enable()

print("Content-Type: text/html\r\n\r\n")

# Parse form data
form = cgi.FieldStorage()
name = form.getvalue("name", "Guest")
email = form.getvalue("email", "Not provided")
message = form.getvalue("message", "No message")

# HTML Template
print(f"""
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>CGI Response - WebServ</title>
    <link rel="stylesheet" href="/assets/css/styles.css">
    <link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700;800&display=swap" rel="stylesheet">
</head>
<body>
    <header>
        <div class="container">
            <nav>
                <a href="/" class="logo">WebServ</a>
                <div class="nav-links">
                    <a href="/">Home</a>
                    <a href="/about.html">About</a>
                    <a href="/services.html">Services</a>
                    <a href="/contact.html">Contact</a>
                    <a href="/cgi-bin/script.py" class="active">CGI Test</a>
                </div>
            </nav>
        </div>
    </header>

    <main>
        <div class="container">
            <div class="card animate-fade-in" style="max-width: 800px; margin: 0 auto;">
                <div style="text-align: center; margin-bottom: 2rem;">
                    <h1 style="font-size: 2.5rem;">CGI Response Received</h1>
                    <p style="color: var(--success);">Script executed successfully!</p>
                </div>

                <div class="grid grid-2">
                    <div>
                        <h3>Server Info</h3>
                        <ul style="color: var(--text-muted);">
                            <li><strong>Method:</strong> {os.environ.get('REQUEST_METHOD', 'Unknown')}</li>
                            <li><strong>Protocol:</strong> {os.environ.get('SERVER_PROTOCOL', 'Unknown')}</li>
                            <li><strong>Python Version:</strong> 3.x</li>
                        </ul>
                    </div>
                    <div>
                        <h3>Form Data</h3>
                        <ul style="color: var(--text-muted);">
                            <li><strong>Name:</strong> {name}</li>
                            <li><strong>Email:</strong> {email}</li>
                            <li><strong>Message:</strong> {message}</li>
                        </ul>
                    </div>
                </div>

                <div class="mt-4 text-center">
                    <a href="/contact.html" class="btn btn-primary">Send Another</a>
                    <a href="/" class="btn btn-outline">Back Home</a>
                </div>
            </div>
        </div>
    </main>

    <footer>
        <div class="container">
            <p>&copy; 2025 WebServ Project. Built with ❤️ by 42 Students.</p>
        </div>
    </footer>
</body>
</html>
""")
