#!/usr/bin/env ruby

# Read form data passed as arguments
name = ARGV[0] || "Unknown"
email = ARGV[1] || "Unknown"
message = ARGV[2] || "No message"

# Create a timestamp for the email log entry
timestamp = Time.now.strftime("%Y-%m-%d %H:%M:%S")

# Format the email content
email_content = <<~EMAIL
  From: #{name} <#{email}>
  Date: #{timestamp}
  Subject: Contact Form Submission

  Message:
  #{message}
  
  ------------------------
EMAIL

# Define the file path where the email content will be saved
log_file = "contact_emails.txt"

# Append the email content to the log file
File.open(log_file, "a") do |file|
  file.puts(email_content)
end

puts "Email content saved to #{log_file}"
