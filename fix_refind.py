conf_path = r'S:\EFI\refind\refind.conf'
with open(conf_path, 'r', encoding='utf-8') as f:
    content = f.read()

new_lines = []
for line in content.split('\n'):
    if line.strip().startswith('scanfor '):
        continue
    new_lines.append(line)

new_content = '\n'.join(new_lines)
new_content += '''
# --- Added for Hackintosh Clean Menu ---
scanfor manual,external,optical

menuentry "Windows" {
    icon \\EFI\\refind\\themes\\rEFInd-glassy\\icons\\os_win.png
    loader \\EFI\\Microsoft\\Boot\\bootmgfw.efi
}

menuentry "macOS" {
    icon \\EFI\\refind\\themes\\rEFInd-glassy\\icons\\os_mac.png
    loader \\EFI\\OC\\OpenCore.efi
}
'''
with open(conf_path, 'w', encoding='utf-8') as f:
    f.write(new_content)
print('Done!')
