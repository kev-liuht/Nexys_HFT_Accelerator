from ouch_parser import OUCHParser
msg = '4f000003b54e000000004141504c20202020000000000017ada3305950594e434c4f52445f4944303031585858000000'
ouch_msg = bytes.fromhex(msg)
parser = OUCHParser()
parsed = parser.decode_message(ouch_msg[0:1], ouch_msg[1:])
print(parsed)