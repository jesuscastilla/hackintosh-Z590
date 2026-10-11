conf_path = r'S:\EFI\refind\refind.conf'
with open(conf_path, 'r', encoding='utf-8') as f:
    content = f.read()

new_content = content.replace('\\EFI\\refind\\themes\\rEFInd-glassy\\icons\\os_win.png', 'EFI/refind/themes/rEFInd-glassy/icons/os_win.png')
new_content = new_content.replace('\\EFI\\Microsoft\\Boot\\bootmgfw.efi', 'EFI/Microsoft/Boot/bootmgfw.efi')
new_content = new_content.replace('\\EFI\\refind\\themes\\rEFInd-glassy\\icons\\os_mac.png', 'EFI/refind/themes/rEFInd-glassy/icons/os_mac.png')
new_content = new_content.replace('\\EFI\\OC\\OpenCore.efi', 'EFI/OC/OpenCore.efi')

with open(conf_path, 'w', encoding='utf-8') as f:
    f.write(new_content)
print('Fixed slashes!')
