msg = '^#request/time#@'
print('msg: ' + msg)
msg_content = msg[1:-1]
print('msg_content: ' + msg_content)
msg_type = msg_content[0]
print('msg_type: ' + msg_type)
msg_purpose = msg_content.split('/')[0][1:]
print('msg_purpose: ' + msg_purpose)
msg_request = msg_content.split('/')[1][:-1]
print('msg_request: ' + msg_request)
