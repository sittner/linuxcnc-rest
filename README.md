# linuxcnc-rest

> [!IMPORTANT]
> **⚠️ This project is archived and no longer maintained.**
>
> The functionality of `linuxcnc-rest` is now part of
> [**@stratuMAK/stratumak**](https://github.com/stratuMAK/stratumak), and all
> further development will continue there. Please use that project instead. This
> repository is kept for historical reference only.

`linuxcnc-rest` is a userspace LinuxCNC HAL component that exposes LinuxCNC HAL
pins and parameters via a REST/JSON API web server. It allows external
applications (GUIs, scripts, PLCs, etc.) to read machine state and send
commands to LinuxCNC over HTTP without requiring a direct HAL connection.

## Features

- Lightweight HTTP REST server bound to `localhost:8080`
- Reads and writes LinuxCNC HAL pins and parameters over HTTP GET/POST
- Supports all standard HAL data types: `bit`, `u32`, `s32`, `float`
- Supports all HAL pin directions (`in`, `out`, `io`) and parameter directions (`ro`, `rw`)
- Declarative XML configuration — no recompilation needed to change pin layout
- Hierarchical JSON structure with nested objects and fixed-size arrays
- Multiple independent REST endpoint roots in a single configuration file
- Clean shutdown on `SIGTERM`/`SIGINT`

## Prerequisites and Dependencies

### Build dependencies

| Dependency | Package (Debian/Ubuntu) |
|---|---|
| LinuxCNC development headers | `linuxcnc-dev` or `linuxcnc-uspace-dev` or `linuxcnc-sim-dev` or `machinekit-dev` |
| Expat XML parser | `libexpat1-dev` |
| Ulfius HTTP framework | `libulfius-dev` |
| Jansson JSON library | *(pulled in by `libulfius-dev`)* |
| debhelper (for packaging) | `debhelper (>= 8.0.0)` |

### Runtime dependencies

| Dependency | Package |
|---|---|
| LinuxCNC | `linuxcnc` or `linuxcnc-uspace` or `linuxcnc-sim` or `machinekit` |
| Ulfius runtime library | `libulfius2.5` or `libulfius2.7` |

Install build dependencies on a Debian/Ubuntu system:

```bash
sudo apt-get install libexpat1-dev libulfius-dev linuxcnc-uspace-dev
```

## Building from Source

The build system uses `halcompile`/`comp` from LinuxCNC to auto-detect the
build environment.

```bash
# Clone the repository
git clone https://github.com/sittner/linuxcnc-rest.git
cd linuxcnc-rest

# Generate config.mk (requires halcompile or comp in PATH)
make configure

# Build the lcrest binary
make
```

The resulting binary is `src/lcrest`.

## Building a Debian Package

```bash
cd linuxcnc-rest
dpkg-buildpackage -b -uc -us
```

The `.deb` package will be created in the parent directory. Install it with:

```bash
sudo dpkg -i ../linuxcnc-rest_*.deb
```

## Installation

```bash
# Install the binary and example files
sudo make install
```

This copies:
- `lcrest` binary → `$(EMC2_HOME)/bin/lcrest`
- Example configuration files → `$(EMC2_HOME)/share/linuxcnc-rest/examples/`

## Usage

### Running lcrest directly

```bash
lcrest /path/to/rest-config.xml
```

`lcrest` takes exactly one argument: the path to an XML configuration file.
It starts the HAL component, exports all configured pins/parameters, starts the
HTTP server on `127.0.0.1:8080`, and runs until `SIGTERM` or `SIGINT`.

### Loading lcrest in a HAL file

Add the following line to your LinuxCNC HAL file (`.hal`):

```hal
loadusr -W lcrest /path/to/rest-config.xml
```

The `-W` flag tells LinuxCNC to wait until `lcrest` signals ready before
continuing with the rest of the HAL file. This ensures all pins are available
before any `net` commands that reference them.

### Connecting HAL pins

After loading, connect the exported pins to other HAL components using `net`:

```hal
loadusr -W lcrest /path/to/rest-config.xml

# Example: connect an output pin from the REST interface to a motion pin
net rest-spindle-enable  json.GuiInMain.spindle-enable  spindle.0.enable
```

## Configuration

The XML configuration file controls which HAL pins are created and how they are
exposed via the REST API. The file must be a valid XML document with the
following structure.

### Root element

```xml
<halJson>
  <!-- one or more halJsonRoot elements -->
</halJson>
```

### `<halJsonRoot path="...">`

Defines a REST endpoint. Each root creates an HTTP endpoint at
`/hal/json/<path>`. Multiple roots can be defined in one file.

| Attribute | Required | Description |
|---|---|---|
| `path` | yes | URL path segment and HAL name prefix for this root |

```xml
<halJsonRoot path="MyEndpoint">
  <!-- pins, params, objects, arrays -->
</halJsonRoot>
```

### `<halJsonPin name="..." type="..." dir="..."/>`

Defines a HAL pin. Each pin becomes a JSON field in the REST response.

| Attribute | Required | Values | Description |
|---|---|---|---|
| `name` | yes | any identifier | JSON key name and HAL pin name suffix |
| `type` | yes | `bit`, `u32`, `s32`, `float` | HAL data type |
| `dir`  | yes | `in`, `out`, `io` | HAL pin direction |

- `in` pins are read-only from the REST perspective (GET only; POST is silently ignored)
- `out` and `io` pins can be written via POST

### `<halJsonParam name="..." type="..." dir="..."/>`

Defines a HAL parameter. Parameters behave like pins in the REST interface.

| Attribute | Required | Values | Description |
|---|---|---|---|
| `name` | yes | any identifier | JSON key name and HAL param name suffix |
| `type` | yes | `bit`, `u32`, `s32`, `float` | HAL data type |
| `dir`  | yes | `ro`, `rw` | HAL parameter direction |

- `ro` parameters are read-only (GET only)
- `rw` parameters can be written via POST

### `<halJsonObject name="...">`

Groups child elements under a nested JSON object. May contain `halJsonPin`,
`halJsonParam`, `halJsonObject`, and `halJsonArray` children.

| Attribute | Required | Description |
|---|---|---|
| `name` | yes | JSON key name and HAL name path segment |

```xml
<halJsonObject name="axis">
  <halJsonPin name="pos" type="float" dir="in"/>
  <halJsonPin name="homed" type="bit" dir="in"/>
</halJsonObject>
```

### `<halJsonArray name="..." size="...">`

Creates a fixed-size JSON array. The single XML element definition is replicated
`size` times at runtime; each replica has its own HAL pin with a numeric index
suffix.

| Attribute | Required | Description |
|---|---|---|
| `name` | yes | JSON key name and HAL name path segment |
| `size` | yes | Number of array elements (positive integer) |

```xml
<halJsonArray name="axes" size="3">
  <halJsonPin name="pos" type="float" dir="in"/>
</halJsonArray>
```

Arrays can be nested inside other arrays or objects.

## REST API Reference

The server listens on `http://127.0.0.1:8080`.

### GET `/hal/json/<path>`

Returns the current values of all HAL pins and parameters under the given root
as a JSON object.

**Response:** `200 OK` with a JSON body.

```bash
curl http://127.0.0.1:8080/hal/json/GuiOutMain
```

Example response:

```json
{
  "errors": 0,
  "ready": true,
  "running": false,
  "feedOverride": 1.0,
  "heightpot": {
    "pos": 0.0,
    "active": false
  },
  "axes": [
    { "pos": 0.0 },
    { "pos": 0.0 },
    { "pos": 0.0 }
  ]
}
```

### POST `/hal/json/<path>`

Writes values to writable HAL pins and parameters. Supply a JSON body with the
same structure as the GET response. Only writable fields (`out`/`io` pins,
`rw` params) are updated; read-only fields are silently ignored.

**Response:** `200 OK` with body `OK`, or `400 Bad Request` on JSON parse error.

```bash
curl -X POST http://127.0.0.1:8080/hal/json/GuiInMain \
     -H "Content-Type: application/json" \
     -d '{"startRef": true, "materialHeight": 18.5}'
```

Partial updates are supported — you only need to include the fields you want
to change. Nested objects and arrays follow the same JSON structure as the GET
response:

```bash
curl -X POST http://127.0.0.1:8080/hal/json/GuiInMain \
     -H "Content-Type: application/json" \
     -d '{
       "heightpot": {
         "calibStart": true
       },
       "faces": [
         {"manu": false, "ena": true},
         {"manu": false, "ena": false}
       ]
     }'
```

## HAL Pin Naming

HAL pin names are constructed from the XML hierarchy using dots (`.`) as
separators. The top-level prefix is always `json`.

**Pattern:**

```
json.<root-path>.<object-or-array-name>.<pin-name>
```

Array elements use a hyphen-index suffix on the array name:

```
json.<root-path>.<array-name>-<index>.<pin-name>
```

**Examples** for a root with `path="GuiInMain"`:

| XML path | HAL pin name |
|---|---|
| `<halJsonPin name="startRef"/>` | `json.GuiInMain.startRef` |
| `<halJsonObject name="heightpot">` → `<halJsonPin name="pos"/>` | `json.GuiInMain.heightpot.pos` |
| `<halJsonArray name="faces" size="3">` → `<halJsonPin name="ena"/>` (index 0) | `json.GuiInMain.faces-0.ena` |
| `<halJsonArray name="faces" size="3">` → `<halJsonPin name="ena"/>` (index 2) | `json.GuiInMain.faces-2.ena` |
| `<halJsonArray name="bevels" size="2">` → `<halJsonArray name="motors" size="3">` → `<halJsonPin name="active"/>` (bevel 1, motor 2) | `json.GuiInMain.bevels-1.motors-2.active` |

You can verify the exported pins at runtime with:

```bash
halcmd show pin json
```

## Example

### Minimal configuration file (`minimal-config.xml`)

```xml
<halJson>

  <!-- Read-only status endpoint -->
  <halJsonRoot path="Status">
    <halJsonPin name="estop" type="bit" dir="in"/>
    <halJsonPin name="enabled" type="bit" dir="in"/>
    <halJsonPin name="feedOverride" type="float" dir="in"/>
    <halJsonObject name="axis0">
      <halJsonPin name="pos" type="float" dir="in"/>
      <halJsonPin name="homed" type="bit" dir="in"/>
    </halJsonObject>
  </halJsonRoot>

  <!-- Write-only command endpoint -->
  <halJsonRoot path="Commands">
    <halJsonPin name="enable" type="bit" dir="out"/>
    <halJsonPin name="feedOverride" type="float" dir="out"/>
  </halJsonRoot>

</halJson>
```

### HAL file snippet

```hal
loadusr -W lcrest /etc/linuxcnc/minimal-config.xml

# Wire status pins (lcrest reads these, LinuxCNC writes them)
net estop-out  halui.estop.is-activated  json.Status.estop
net machine-on halui.machine.is-on       json.Status.enabled

# Wire command pins (lcrest writes these, LinuxCNC reads them)
net enable-cmd json.Commands.enable  motion.enable
```

### Corresponding curl commands

```bash
# Read machine status
curl http://127.0.0.1:8080/hal/json/Status

# Enable the machine
curl -X POST http://127.0.0.1:8080/hal/json/Commands \
     -H "Content-Type: application/json" \
     -d '{"enable": true}'

# Set feed override to 80%
curl -X POST http://127.0.0.1:8080/hal/json/Commands \
     -H "Content-Type: application/json" \
     -d '{"feedOverride": 0.8}'
```

## Project Structure

```
linuxcnc-rest/
├── configure.mk          # Build configuration detection (uses halcompile/comp)
├── Makefile              # Top-level build entry point
├── src/
│   ├── user.mk           # Compiler flags and link rules
│   ├── Makefile          # Source-level build entry point
│   ├── lcrest.h          # Shared declarations (modname)
│   ├── lcrest_main.c     # Entry point: argument parsing, HAL init, signal handling
│   ├── lcrest_conf.c     # XML configuration parser (Expat-based)
│   ├── lcrest_conf.h     # Configuration data structures
│   ├── lcrest_hal.c      # HAL pin/parameter export, read, and write
│   ├── lcrest_hal.h      # HAL helper declarations
│   ├── lcrest_rest.c     # Ulfius HTTP server setup and request callbacks
│   ├── lcrest_rest.h     # REST server declarations
│   ├── lcrest_json.c     # JSON response builder and request parser (Jansson)
│   └── lcrest_json.h     # JSON helper declarations
├── examples/
│   └── json/
│       └── rest-config.xml   # Example configuration with multiple roots
├── debian/               # Debian packaging files
├── ChangeLog
└── LICENSE               # GNU General Public License v2.0
```

## License

This project is licensed under the **GNU General Public License v2.0**.
See the [LICENSE](LICENSE) file for the full license text.
