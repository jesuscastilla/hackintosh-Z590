# Guía de Instalación: AMD Radeon RX 5700 XT y Configuración de iServices (iMessage)

Documento técnico de referencia para la transición a la GPU AMD Radeon RX 5700 XT, restauración del snapshot sellado nativo de Apple (SIP 100%) y reconfiguración de iMessage / iServices en macOS Tahoe (ASUS PRIME Z590-P + Intel Core i5-11600K).

---

## PARTE 1: Instalación de la RX 5700 XT y Sellado del Sistema

### Fase 1: Preparación previa del arranque (en Windows)

Antes de apagar el ordenador, activamos el perfil de la RX 5700 XT (`config.rx5700xt.plist`) en la partición EFI de arranque.

1. Abre **PowerShell como Administrador** en Windows.
2. Ejecuta el siguiente bloque:

```powershell
# 1. Asignar letra S: a la partición EFI del disco de macOS
Add-PartitionAccessPath -DiskNumber 2 -PartitionNumber 1 -AccessPath "S:\"

# 2. Reemplazar config.plist por el perfil optimizado de la RX 5700 XT
Copy-Item "S:\EFI\OC\config.rx5700xt.plist" "S:\EFI\OC\config.plist" -Force
Copy-Item "G:\Hackintosh\EFI\OC\config.rx5700xt.plist" "G:\Hackintosh\EFI\OC\config.plist" -Force

# 3. Desmontar la unidad S: limpiamente
Remove-PartitionAccessPath -DiskNumber 2 -PartitionNumber 1 -AccessPath "S:\"
```

3. Apaga el ordenador.

---

### Fase 2: Instalación física de la tarjeta gráfica

1. **Seguridad**: Apaga el interruptor de la fuente de alimentación (PSU) y desenchufa el cable de red eléctrica.
2. **Extracción**:
   - Desconecta los conectores PCIe de la Nvidia GTX 970.
   - Retira el tornillo de sujeción al chasis y libera la palanca de seguridad del zócalo PCIe.
   - Retira la GTX 970.
3. **Colocación**:
   - Inserta la **AMD Radeon RX 5700 XT en la ranura PCIe 1** (la superior reforzada, PCIe 4.0 x16 directa a la CPU).
   - Atornilla la tarjeta al chasis.
   - Conecta con firmeza los cables de alimentación PCIe de la fuente (8 pines + 6 pines u 8+8 pines).
4. **Monitor y periféricos**:
   - Conecta el cable DisplayPort o HDMI **directamente a las salidas de la RX 5700 XT**.
   - Verifica que las antenas Wi-Fi/Bluetooth sigan firmemente enroscadas a los conectores dorados de la placa base (Intel AX201).

---

### Fase 3: Primer arranque con la nueva gráfica

1. Enciende el equipo.
2. En el selector de arranque de OpenCore:
   - Pulsa la tecla **Espacio** para mostrar las herramientas auxiliares.
   - Selecciona **Reset NVRAM**. El equipo se reiniciará automáticamente. *(Obligatorio para vaciar cachés de framebuffer previas y forzar la renegociación de pantalla)*.
3. Al reiniciar, selecciona tu disco de **macOS**.
4. El sistema cargará con **aceleración gráfica nativa completa (`AMDRadeonX6000`)** y soporte Metal 3 gracias al argumento `agdpmod=pikera`.

---

### Fase 4: Revertir parches de OCLP y restaurar el APFS Snapshot sellado

Con la RX 5700 XT ya no se necesita ningún parche en el sistema de archivos:

1. Inicia sesión en macOS.
2. Abre la aplicación **OpenCore Legacy Patcher (OCLP)**.
3. Pulsa en **Post-Install Root Patch**.
4. Pulsa en **Revert Root Patches**.
5. Introduce tu contraseña de administrador.
6. Espera a que el proceso termine de restaurar la **instantánea raíz sellada por Apple** (`com.apple.os.update-...`).
7. Haz clic en **Reboot** para reiniciar el equipo.

---

### Fase 5: Ajuste de estabilidad y seguridad (SIP)

Una vez iniciado el sistema en la instantánea nativa limpia:

1. Monta la partición EFI y edita `config.plist`:
   - **NVRAM -> Add -> 7C436110-AB2A-4BBB-A880-FE41995C9F82**:
     - `csr-active-config`: se mantiene en `030A0000` o `00000000` según si usas kexts no firmados como SimpleHDA.
     - `boot-args`: conserva `amfi=0x80` para permitir la inyección de SimpleHDA sin bloqueos de AMFI.
     - *(Opcional)*: si no quieres ver texto de arranque técnico, elimina `-v`.
   - **Misc -> Security**:
     - `SecureBootModel`: debe permanecer **siempre en `Disabled`** porque el perfil `iMac20,1` emula un Mac con chip de seguridad T2 de Apple (`j185`), el cual no existe físicamente en placas PC y causa un Kernel Panic inmediato si se pone en `Default`.
2. Guarda los cambios y reinicia.

---

## PARTE 2: Reconfiguración de iMessage, FaceTime y Apple ID (iServices)

Para que iMessage y FaceTime funcionen sin bloqueos ("Error de autenticación" o "Póngase en contacto con el soporte técnico de Apple"), se requieren tres pilares:

### Pilar 1: La tarjeta de red principal (`en0`) debe ser `Built-in`

macOS utiliza la dirección MAC de la interfaz de red interna `en0` como clave criptográfica única (G-GUID).

1. Abre **Terminal** en macOS y comprueba tus interfaces de red:
```bash
networksetup -listallhardwareports
```
2. La salida **debe mostrar** que el puerto `Ethernet` es `en0`:
```text
Hardware Port: Ethernet
Device: en0
```
3. Comprueba que el sistema lo reconoce como interno:
```bash
ioreg -l | grep -A 5 -B 5 "IOBuiltin"
```
*(Debe devolver `"IOBuiltin" = Yes`)*.
> [!NOTE]
> En tu OpenCore ya está inyectada la propiedad `built-in = 01` en `PciRoot(0x0)/Pci(0x1C,0x7)/Pci(0x0,0x0)` (el controlador Realtek 2.5 GbE), asegurando que siempre sea reconocido como interno.

Si por algún motivo `en0` lo tomó el Wi-Fi u otra tarjeta, repara las interfaces ejecutando en la Terminal:
```bash
sudo rm -f /Library/Preferences/SystemConfiguration/NetworkInterfaces.plist
sudo rm -f /Library/Preferences/SystemConfiguration/preferences.plist
```
*(Al reiniciar el Mac, macOS asignará de nuevo `en0` al puerto Ethernet interno).*

---

### Pilar 2: Generación y Validación de Seriales (SMBIOS iMac20,1)

Tu placa base ASUS Z590 con CPU i5-11600K utiliza el perfil **`iMac20,1`**. Si necesitas regenerar credenciales limpias:

1. Desde Windows (o macOS), puedes usar la herramienta `macserial` incluida en el repositorio:
```powershell
# En PowerShell:
& "G:\Hackintosh\OpenCore\Utilities\macserial\macserial.exe" -m "iMac20,1" -n 1
```
Esto generará una pareja de valores con el formato:
`SystemSerialNumber | MLB` (Ejemplo: `C02GN8ZLPN5T | C02145700GUPHC1UE`).

2. **Generar un nuevo SystemUUID**:
En PowerShell:
```powershell
[guid]::NewGuid().ToString().ToUpper()
```

3. **Verificar el número de serie en Apple**:
Visita: [https://checkcoverage.apple.com](https://checkcoverage.apple.com)
- **Resultado ideal para Hackintosh**: Debe decir *"No podemos comprobar la cobertura de este número de serie"* o *"Fecha de compra no validada"*.
- **NO USAR**: Si la web reconoce el Mac y dice que está registrado con una fecha de compra activa o AppleCare, **no lo uses**, pertenece a un Mac real. Genera otro hasta encontrar uno no registrado.

4. **El valor ROM**:
Debe coincidir con la dirección MAC física de tu tarjeta Ethernet Realtek 2.5 GbE (`FC:34:97:A6:C2:EA`). En `config.plist` se introduce como `<data>/DSXpsLq</data>` (en binario `FC 34 97 A6 C2 EA`).

5. Introduce estos 4 valores en tu `config.plist` bajo **PlatformInfo -> Generic**:
   - `SystemProductName`: `iMac20,1`
   - `SystemSerialNumber`: Tu nuevo serial
   - `MLB`: Tu nuevo MLB (17 caracteres)
   - `SystemUUID`: Tu nuevo UUID
   - `ROM`: Tu MAC de Ethernet en formato data

---

### Pilar 3: Purgar cachés viejas de Apple ID / iMessage en macOS

Si iMessage se quedó atascado con una configuración anterior, hay que limpiar las credenciales almacenadas en caché:

1. Cierra sesión en:
   - **Ajustes del Sistema -> ID de Apple**
   - **Mensajes -> Ajustes -> Cerrar sesión**
   - **FaceTime -> Ajustes -> Cerrar sesión**
2. Abre la **Terminal** de macOS y ejecuta la siguiente limpieza completa:

```bash
# Borrado de cachés de Mensajes e iChat
rm -rf ~/Library/Caches/com.apple.iCloudHelper
rm -rf ~/Library/Caches/com.apple.Messages
rm -rf ~/Library/Caches/com.apple.imfoundation.IMRemoteURLConnectionAgent
rm -rf ~/Library/Preferences/com.apple.iChat*
rm -rf ~/Library/Preferences/com.apple.imagent*
rm -rf ~/Library/Preferences/com.apple.immessage*
rm -rf ~/Library/Preferences/com.apple.imservice*
killall cfprefsd
```

3. **Limpieza del Llavero (Keychain)**:
   - Abre la app **Acceso a Llaveros** (Keychain Access).
   - En la barra de búsqueda busca y elimina los siguientes certificados y claves si existen:
     - `ids: identity-rsa-private-key`
     - `ids: identity-rsa-public-key`
     - `com.apple.facetime: registrationInfo`
4. **Reinicio**:
   - Reinicia el Mac.
   - Realiza un **Reset NVRAM** en OpenCore.
5. **Inicio de sesión**:
   - Entra en macOS.
   - Ve a **Ajustes del Sistema -> Iniciar sesión con el ID de Apple** (introduce el código 2FA en tu móvil/correo).
   - Abre la app **Mensajes** e inicia sesión con tu ID de Apple. Conectará de inmediato sin errores.
