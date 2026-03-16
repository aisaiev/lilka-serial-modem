#include "webserver.h"
#include "config.h"
#include "display_ui.h"
#include "ftp_server.h"
#include "modem.h"
#include "network.h"
#include "storage.h"
#include <SD.h>
#include <WiFi.h>
#include <lilka.h>

WebServer webServer;

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>Lilka Serial Modem</title>
    <link rel="icon" type="image/png" href="data:image/png;base64,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aHR0cDovL25zLmFkb2JlLmNvbS94YXAvMS4wLyIKICAgeG1wTU06RG9jdW1lbnRJRD0iZ2ltcDpkb2NpZDpnaW1wOmJhNTI4ODFkLTU4ODYtNGZmNS05MzhiLTI2MDJlNmI1ZGMzNyIKICAgeG1wTU06SW5zdGFuY2VJRD0ieG1wLmlpZDozNGViNzlkZC1mNGNjLTRiYjItOTM5Ny1iZDcyYzA2ZDkwNzQiCiAgIHhtcE1NOk9yaWdpbmFsRG9jdW1lbnRJRD0ieG1wLmRpZDozN2ZiNDQwMi00ODY3LTQzYmMtOTc5Yy1iNmIyMDZlY2MyNDciCiAgIGRjOkZvcm1hdD0iaW1hZ2UvcG5nIgogICBHSU1QOkFQST0iMi4wIgogICBHSU1QOlBsYXRmb3JtPSJXaW5kb3dzIgogICBHSU1QOlRpbWVTdGFtcD0iMTY3MzQyNjAwNjgzNDIxNiIKICAgR0lNUDpWZXJzaW9uPSIyLjEwLjMwIgogICB0aWZmOk9yaWVudGF0aW9uPSIxIgogICB4bXA6Q3JlYXRvclRvb2w9IkdJTVAgMi4xMCI+CiAgIDx4bXBNTTpIaXN0b3J5PgogICAgPHJkZjpTZXE+CiAgICAgPHJkZjpsaQogICAgICBzdEV2dDphY3Rpb249InNhdmVkIgogICAgICBzdEV2dDpjaGFuZ2VkPSIvIgogICAgICBzdEV2dDppbnN0YW5jZUlEPSJ4bXAuaWlkOjc5ZDYwNTBiLTkxMmYtNGE4Ni05N2RkLTIzZmUyNjY0NzU4YyIKICAgICAgc3RFdnQ6c29mdHdhcmVBZ2VudD0iR2ltcCAyLjEwIChXaW5kb3dzKSIKICAgICAgc3RFdnQ6d2hlbj0iMjAyMy0wMS0xMVQwMzozMzoyNiIvPgogICAgPC9yZGY6U2VxPgogICA8L3htcE1NOkhpc3Rvcnk+CiAgPC9yZGY6RGVzY3JpcHRpb24+CiA8L3JkZjpSREY+CjwveDp4bXBtZXRhPgogICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgCiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAKICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIAogICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgCiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAKICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIAogICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgCiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAKICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIAogICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgCiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAKICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIAogICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgCiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAKICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIAogICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgCiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAKICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIAogICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgCiAgICAgICAg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">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        :root { font-size: 100%; }
        @font-face {
            font-family: monospace;
        }
        body {
            background-color: rgb(111, 198, 213);
            color: rgb(26, 26, 26);
            font-family: monospace;
            max-width: 768px;
            margin: auto;
        }
        button {
            border: 2px outset;
            font-family: monospace;
        }
        button:focus {
            border: 2px inset;
        }
        .container {
            margin: 6px;
            padding: 6px;
            border: 6px double rgb(139, 139, 139);
            background-color: rgb(219, 219, 213);
        }
        li {
            margin: 2px;
            display: flex;
            flex-direction: row;
            justify-content: space-between;
        }
        .lowered-box {
            margin: 6px;
            padding: 6px;
            border: 2px inset rgb(237, 237, 237);
            background-color: rgb(219, 219, 213);
        }
        .raised-box {
            margin: 6px;
            padding: 6px;
            font-weight: bold;
            border: 3px outset rgb(237, 237, 237);
            background-color: rgb(219, 219, 213);
        }
        .dial-form { display: flex; gap: 5px; flex-wrap: wrap; }
        .dial-form form { display: flex; gap: 5px; }
        .w-140px { width: 140px; }
        .mt-5px { margin-top: 5px; }
        .links { text-align: right; }
        #title-banner {
            display: flex;
            justify-content: space-between;
            align-items: center;
        }
        #speed-dial-list {
            display: flex;
            flex-wrap: wrap;
        }
        #speed-dial-list > div { margin: 4px; }
        #busymessage { width: 177px; }
        #settings-list { padding-inline-start: 0px; }
        .settings-list-item-title { font-weight: bold; }
        .file-manager-header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 6px;
        }
        .file-manager-buttons { display: flex; gap: 4px; }
        #file-list { list-style: none; padding: 0; }
        #file-list li { display: flex; justify-content: space-between; align-items: center; padding: 4px; }
        #file-list button { margin-left: 10px; }
        #progress-bar-container { margin: 6px; }
        #progress-bar { height: 20px; border: 2px inset rgb(237, 237, 237); background-color: white; }
        #progress-bar-meter { height: 100%; background-color: rgb(0, 0, 170); }
        #progress-bar-text { text-align: center; margin-top: 4px; }
    </style>
</head>
<body>
    <div id="title-banner" class="container">
        <span style="font-weight:bold;font-size:16px;">
            <a href="https://github.com/aisaiev" target="_blank">Serial Modem</a> 
        </span>
        <div class="dial-form">
            <form action="/dial" method="POST">
                <input type="text" name="address" placeholder="address:port" autocomplete="off" required>
                <button type="submit">CALL</button>
            </form>
            <form action="/ath" method="POST">
                <button type="submit">HANG UP</button>
            </form>
        </div>
        <img id="modem-icon" src="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAC0AAAAnCAMAAACsXzHOAAAAIGNIUk0AAHomAACAhAAA+gAAAIDoAAB1MAAA6mAAADqYAAAXcJy6UTwAAABgUExURcDAwMwzM2ZmZpmZmczMzAAAAAD//wBmZgCZmWaZmZlmZsyZmf//AJnMzGaZAJlmAGZmAGaZZv///wAA/5mZAMyZzAD/AMzMmWZmmZmZZv8AAJlmmZnMmQCZZgBmmcDAwNIx8LUAAAABdFJOUwBA5thmAAAAAWJLR0QAiAUdSAAAAAlwSFlzAAAAyAAAAMgAY/rnrQAAAAd0SU1FB+oBDw8QNQklOu0AAAABb3JOVAHPoneaAAABjUlEQVQ4y62U65KCMAyFoU3E1qKUguKivv9jmrZULumuzo7p8O/rITnkUBRfqPLfePl1uvwEZhCDhUwnQ21pCakEMoxLA8aSEhnAaIJ2VbWvqqj9ZnDS3u2J3oNKtFZ08iZJwCrQh0QrbaiW/GyoSDToSBOrUC/4slx6giq8up5oZQwaPAZ9NqmAA7lX0zN1ohGJxxP6a8wTfFU0RRlowFjSNwwX7VwanXPaw11vDOCJ0bKF87mGA0mHVoJycyG4wePpiGxLlJ4WhcbUpm1NBx7uGt/dpm9cVDDE8wHumPS2tB/NtAEmbQiLVv6y5KptvX/Gw8S+VqfM3tAAgQ+6l+E6/NmKgp/Ie9aOVjJ6tOnQyP2t93xgr8OVeWJX2SFR4m/xNvI+xk12/HDg7SfWMdr2Pjv3x5wd4gNbZOjxRtl53B/3OTtF+oYZ7ZSG3rI2Xa7vSF/GD2g7ZUdkMp/TXmfHbZ6N9ryCPphucTg9LrIj0BXrw6R9dqAO2ZH4po+wJYOO/wiBb3/s6+x8sZ6eChkjYjjZjAAAACV0RVh0ZGF0ZTpjcmVhdGUAMjAyNi0wMS0xNVQxNToxNjo1MiswMDowMMe/SNAAAAAldEVYdGRhdGU6bW9kaWZ5ADIwMjYtMDEtMTVUMTU6MTY6NTIrMDA6MDC24vBsAAAAKHRFWHRkYXRlOnRpbWVzdGFtcAAyMDI2LTAxLTE1VDE1OjE2OjUzKzAwOjAwR4DaBwAAAABJRU5ErkJggg==" alt="Modem Icon">
    </div>
    
    <div class="container">
        <div class="lowered-box">
            <b>WIFI STATUS:</b> <span id="wifi-status"></span><br>
            <b>SSID.......:</b> <span id="ssid-status"></span><br>
            <b>MAC ADDRESS:</b> <span id="mac-address"></span><br>
            <b>IP ADDRESS.:</b> <span id="ip-address"></span><br>
            <b>GATEWAY....:</b> <span id="gateway"></span><br>
            <b>SUBNET MASK:</b> <span id="subnet"></span><br>
            <b>BAUD.......:</b> <span id="baud-status"></span><br>
            <b>BUILD......:</b> <span id="build-status"></span><br>
            <b>CALL STATUS:</b> <span id="call-status"></span><br>
            <b>CALL LENGTH:</b> <span id="call-length"></span><br>
        </div>
    </div>
    
    <div class="container">
        <div class="raised-box">SETTINGS</div>
        <div class="lowered-box">
            <form action="/save-settings" method="POST">
                <ul id="settings-list">
                    <li>
                        <label for="baud" class="settings-list-item-title">BAUD</label>
                        <select name="baud" id="baud">
                            <option value="0">300</option>
                            <option value="1">1200</option>
                            <option value="2">2400</option>
                            <option value="3">4800</option>
                            <option value="4">9600</option>
                            <option value="5">19200</option>
                            <option value="6">38400</option>
                            <option value="7">57600</option>
                            <option value="8">115200</option>
                        </select>
                    </li>
                    <li>
                        <label for="echo" class="settings-list-item-title">ECHO</label>
                        <input name="echo" id="echo" value="1" type="checkbox">
                    </li>
                    <li>
                        <label for="quiet" class="settings-list-item-title">QUIET MODE</label>
                        <input name="quiet" id="quiet" value="1" type="checkbox">
                    </li>
                    <li>
                        <label for="verbose" class="settings-list-item-title">VERBOSE MODE</label>
                        <input name="verbose" id="verbose" value="1" type="checkbox">
                    </li>
                    <li>
                        <label for="telnet" class="settings-list-item-title">HANDLE TELNET</label>
                        <input name="telnet" id="telnet" value="1" type="checkbox">
                    </li>
                    <li>
                        <label for="autoanswer" class="settings-list-item-title">AUTO ANSWER</label>
                        <input name="autoanswer" id="autoanswer" value="1" type="checkbox">
                    </li>
                    <li>
                        <label for="ftp" class="settings-list-item-title">FTP SERVER</label>
                        <input name="ftp" id="ftp" value="1" type="checkbox">
                    </li>
                    <li>
                        <label for="buzzer" class="settings-list-item-title">BUZZER SOUNDS</label>
                        <input name="buzzer" id="buzzer" value="1" type="checkbox">
                    </li>
                    <li>
                        <label for="serverport" class="settings-list-item-title">TCP SERVER PORT</label>
                        <input name="serverport" id="serverport" type="number" min="0" max="65535" style="width:80px;">
                    </li>
                    <li>
                        <label for="busymsg" class="settings-list-item-title">BUSY MESSAGE</label>
                        <textarea name="busymsg" id="busymsg" cols="20" rows="5" maxlength="100"></textarea>
                    </li>
                </ul>
                <button type="submit" class="w-140px">Save Settings</button>
            </form>
            <form action="/reload-settings" method="POST">
                <button type="submit" class="w-140px mt-5px">Reload Settings</button>
            </form>
        </div>
        <div class="links">
            
            <form action="/factory-defaults" method="POST" style="display:inline;" onsubmit="return confirm('If you reset factory defaults you will not be able to access this web interface until you configure WIFI again. Are you sure?');">
                <button type="submit" style="background:none;border:none;color:blue;text-decoration:underline;cursor:pointer;font-family:monospace;">FACTORY DEFAULTS</button>
            </form>
        </div>
    </div>
    
    <div class="container">
        <div class="raised-box">FILE MANAGER</div>
        <div class="lowered-box">
            <div class="file-manager-header">
                <div id="current-path" style="padding: 4px; font-weight: bold;">/</div>
                <div class="file-manager-buttons">
                    <button type="button" onclick="createFolder()">CREATE DIR</button>
                    <button type="button" onclick="getFileList()">REFRESH</button>
                </div>
            </div>
            <button id="back-button" onclick="navigateUp()" style="display: none;">&lt;- BACK</button>
            <ul id="file-list"></ul>
        </div>
        <div class="raised-box">
            <div style="margin-bottom: 6px">Upload File</div>
            <input id="file-selector" type="file" name="name">
            <input type="submit" value="UPLOAD" onclick="httpUpload()">
        </div>
        <div id="progress-bar-container">
            <div id="progress-bar">
                <div id="progress-bar-meter" style="width: 0%;"></div>
            </div>
            <div id="progress-bar-text">
                <span id="used-bytes">0</span>
                <span> / </span>
                <span id="total-bytes">0</span>
            </div>
        </div>
    </div>
    
    <div class="container">
        <form action="/update-speeddial" method="POST">
            <div class="raised-box">SPEED DIAL</div>
            <div class="lowered-box">
                <div id="speed-dial-list"></div>
                <input type="submit" value="SAVE">
            </div>
        </form>
    </div>
    
    <script>
        // Format bytes to human-readable format
        const formatBytes = (bytes) => {
            if (bytes < 1024) return bytes + ' B';
            if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + ' KB';
            if (bytes < 1024 * 1024 * 1024) return (bytes / (1024 * 1024)).toFixed(1) + ' MB';
            return (bytes / (1024 * 1024 * 1024)).toFixed(2) + ' GB';
        };
        
        const wifiStatus = document.getElementById('wifi-status');
        const ssidStatus = document.getElementById('ssid-status');
        const macAddress = document.getElementById('mac-address');
        const ipAddress = document.getElementById('ip-address');
        const gateway = document.getElementById('gateway');
        const subnet = document.getElementById('subnet');
        const callStatus = document.getElementById('call-status');
        const callLength = document.getElementById('call-length');
        const baudStatus = document.getElementById('baud-status');
        const buildStatus = document.getElementById('build-status');
        
        const baud = document.getElementById('baud');
        const echo = document.getElementById('echo');
        const quiet = document.getElementById('quiet');
        const verbose = document.getElementById('verbose');
        const telnet = document.getElementById('telnet');
        const autoanswer = document.getElementById('autoanswer');
        const ftp = document.getElementById('ftp');
        const serverport = document.getElementById('serverport');
        const busymsg = document.getElementById('busymsg');
        const buzzer = document.getElementById('buzzer');
        
        // Server-Sent Events for real-time status updates
        const statusEventSource = new EventSource('/events');

        statusEventSource.addEventListener('status', (event) => {
            const data = JSON.parse(event.data);
            wifiStatus.innerText = data.wifiStatus;
            ssidStatus.innerText = data.ssidStatus;
            macAddress.innerText = data.macAddress;
            ipAddress.innerText = data.ipAddress;
            gateway.innerText = data.gateway;
            subnet.innerText = data.subnet;
            callStatus.innerText = data.callStatus;
            callLength.innerText = data.callLength;
            baudStatus.innerText = data.baudStatus;
            buildStatus.innerText = data.build;
        });
        
        const getSettings = () => {
            return fetch('/get-settings')
            .then((response) => response.json())
            .then((data) => {
                baud.value = data.serialspeed;
                telnet.checked = Boolean(parseInt(data.telnet));
                verbose.checked = Boolean(parseInt(data.verboseResults));
                quiet.checked = Boolean(parseInt(data.quietMode));
                echo.checked = Boolean(parseInt(data.echo));
                autoanswer.checked = Boolean(parseInt(data.autoAnswer));
                ftp.checked = Boolean(parseInt(data.ftpEnabled));
                serverport.value = data.serverPort;
                busymsg.value = data.busyMsg;
                buzzer.checked = Boolean(parseInt(data.buzzer));
            });
        };
        
        const getSpeedDial = () => {
            return fetch('/get-speeddials')
            .then((response) => response.json())
            .then((data) => {
                const speedDialList = document.getElementById('speed-dial-list');
                data.forEach((number, index) => {
                    const div = document.createElement("div");
                    const label = document.createElement("label");
                    label.htmlFor = index;
                    label.style.marginRight = "4px";
                    label.style.fontWeight = "bold";
                    label.innerText = index;
                    const input = document.createElement("input");
                    input.type = "text";
                    input.value = number;
                    input.name = index;
                    input.id = index;
                    div.appendChild(label);
                    div.appendChild(input);
                    speedDialList.appendChild(div);
                });
            });
        };
        
        let currentPath = '/';
        
        const getFileList = (path) => {
            if (path !== undefined) currentPath = path;
            return fetch('/list-files?path=' + encodeURIComponent(currentPath))
            .then((response) => response.json())
            .then((data) => {
                const fileList = document.getElementById('file-list');
                const pathDisplay = document.getElementById('current-path');
                const backButton = document.getElementById('back-button');
                const usedBytes = document.getElementById('used-bytes');
                const totalBytes = document.getElementById('total-bytes');
                const progressMeter = document.getElementById('progress-bar-meter');
                
                pathDisplay.innerText = currentPath;
                backButton.style.display = currentPath === '/' ? 'none' : 'inline-block';
                
                fileList.innerHTML = '';
                
                if (data.files && data.files.length > 0) {
                    // Sort: directories first, then files
                    data.files.sort((a, b) => {
                        if (a.isDir !== b.isDir) return b.isDir - a.isDir;
                        return a.name.localeCompare(b.name);
                    });
                    
                    data.files.forEach((file) => {
                        const li = document.createElement('li');
                        
                        if (file.isDir) {
                            const folderIcon = document.createElement('img');
                            folderIcon.src = 'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAADAAAAAwCAYAAABXAvmHAAAAAXNSR0IArs4c6QAAAFBlWElmTU0AKgAAAAgAAgESAAMAAAABAAEAAIdpAAQAAAABAAAAJgAAAAAAA6ABAAMAAAABAAEAAKACAAQAAAABAAAAMKADAAQAAAABAAAAMAAAAADJ6kISAAABWWlUWHRYTUw6Y29tLmFkb2JlLnhtcAAAAAAAPHg6eG1wbWV0YSB4bWxuczp4PSJhZG9iZTpuczptZXRhLyIgeDp4bXB0az0iWE1QIENvcmUgNi4wLjAiPgogICA8cmRmOlJERiB4bWxuczpyZGY9Imh0dHA6Ly93d3cudzMub3JnLzE5OTkvMDIvMjItcmRmLXN5bnRheC1ucyMiPgogICAgICA8cmRmOkRlc2NyaXB0aW9uIHJkZjphYm91dD0iIgogICAgICAgICAgICB4bWxuczp0aWZmPSJodHRwOi8vbnMuYWRvYmUuY29tL3RpZmYvMS4wLyI+CiAgICAgICAgIDx0aWZmOk9yaWVudGF0aW9uPjE8L3RpZmY6T3JpZW50YXRpb24+CiAgICAgIDwvcmRmOkRlc2NyaXB0aW9uPgogICA8L3JkZjpSREY+CjwveDp4bXBtZXRhPgoZXuEHAAADZUlEQVRoBe2ZS27bQBBEpSCAdR8fyLvoTNYuB9J9skvmkXxEaTikLIskEEANSNVT011dTfkD2IfDfx7HOf+fn4e/c3dz/Pnc3cxqzvU9w/9sNWP+4+NP6+oOdzqUJVh8tyUmgzT/9vb7jtn29eVyZgliot3ueI69GfKsea3sucS4wFrm916iW2Bt83sucdzK/F5LdAu8v/9y3iZ4vV5W1c0fEuMnsOqEHcROpxNTjuPvge/+2NzM67Uovw/q5oHlS7/7nfNjMwP3hDFjmCfW5qmFc4mhd/8FNDlnUKNgmiX3rEahtl/AYaLGWwblRJ84Z8JzaGy/gMNEjLSWSXPcZz099T1cie0X6Of072ncPFHToouAmXu/2QJpCuueY/D4hOVA60BecmDm1hV6m0+AYUSNDp7DNJk59bySQ7/EugtorNfu35OrF0pD1smlhn1y1pbzugvkIIekIblEe6zjzlzUuGhPOY+/ib37Fjo0m2PI5EtJY9a4UJ6zRv3EYdY6n0AORlhDS5g95BoejE2gYZ6a5xbQoNM0tYQzRibfpFmnnpzznl4ghUN08ROwB9TQEqZu9gz8Ot8DmkJUM+Ty9WBqiJrv2dv3rLFPLJWPfwnZLDLOHNR0ctRoxNyzaG8L6ZG3Hyzx+AIaFFExF+UcCiZXn+njVdd7Vtc+sbQ8vgBGMkLs5pOgxsFiGiL3XOfWg3mXmuQlvr4AQoY56DBzkdqsk5fj3l5z7+paF6HOGvISX1+gHka3nANbnHeaqNF7saXJXWpbU7jlBWzs22+3zzsF5UA5UF4d0RrrQXNqMk8d+CGWF8gBNKRI5grKZZ8mWihnX85IDficwXmI5QWsUoyzucPnOO+tVyuRO+rSnLlovyg/6LQXyKK5vBakTkODeAfweWdNIoWtM7zzRbiI9gKIGXWukNgazF3ymecdM1rn5MlrD3BDtBfwFtSoqJioAWs9J2buMtST12d5kD7CfvOO7N+mC2QTNRoV4bJG3iEaSjSn15BTyzP3yVkv53nA6QIIEaKNiQ7LGjl7qZcz90yNkRpwnrNWzp7A2wU0SYG5zQrW5xDrUvvkrYfnledakx77xeTI7ScvMf55fe0/gffy277zZ3b+Q/Pwv1O3tfWQ+vgvsoe6XsWvJ/B6Aq8n8HoCPoF/9w9ZzLzKbJ0AAAAASUVORK5CYII=';
                            folderIcon.alt = 'folder';
                            folderIcon.style.width = '16px';
                            folderIcon.style.height = '16px';
                            folderIcon.style.marginRight = '4px';
                            folderIcon.style.verticalAlign = 'middle';
                            
                            const folderLink = document.createElement('a');
                            folderLink.href = 'javascript:void(0)';
                            folderLink.appendChild(folderIcon);
                            folderLink.appendChild(document.createTextNode(file.name));
                            folderLink.onclick = () => navigateInto(file.name);
                            li.appendChild(folderLink);
                            
                            const deleteBtn = document.createElement('button');
                            deleteBtn.innerText = 'X';
                            deleteBtn.onclick = () => deleteFile(file.path);
                            li.appendChild(deleteBtn);
                        } else {
                            const link = document.createElement('a');
                            link.href = '/download-file?path=' + encodeURIComponent(file.path);
                            link.innerText = file.name + ' (' + formatBytes(file.size) + ')';
                            
                            const deleteBtn = document.createElement('button');
                            deleteBtn.innerText = 'X';
                            deleteBtn.onclick = () => deleteFile(file.path);
                            
                            li.appendChild(link);
                            li.appendChild(deleteBtn);
                        }
                        
                        fileList.appendChild(li);
                    });
                } else {
                    const li = document.createElement('li');
                    li.innerText = 'No files found';
                    fileList.appendChild(li);
                }
                
                if (data.total && data.used !== undefined) {
                    usedBytes.innerText = formatBytes(data.used);
                    totalBytes.innerText = formatBytes(data.total);
                    const percentage = data.total > 0 ? (data.used / data.total) * 100 : 0;
                    progressMeter.style.width = percentage + '%';
                }
            });
        };
        
        const navigateInto = (folderName) => {
            if (currentPath.endsWith('/')) {
                currentPath = currentPath + folderName;
            } else {
                currentPath = currentPath + '/' + folderName;
            }
            getFileList();
        };
        
        const navigateUp = () => {
            const lastSlash = currentPath.lastIndexOf('/');
            if (lastSlash === 0) {
                currentPath = '/';
            } else {
                currentPath = currentPath.substring(0, lastSlash);
            }
            getFileList();
        };
        
        const createFolder = () => {
            const folderName = prompt('Enter folder name:');
            if (!folderName || folderName.trim() === '') return;
            
            if (folderName.indexOf('/') >= 0 || folderName.indexOf('..') >= 0) {
                alert('Invalid folder name');
                return;
            }
            
            fetch('/create-folder', {
                method: 'POST',
                headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
                body: 'path=' + encodeURIComponent(currentPath) + '&name=' + encodeURIComponent(folderName)
            })
            .then((response) => {
                if (response.ok) {
                    getFileList();
                } else {
                    alert('Failed to create folder');
                }
            })
            .catch((e) => alert('Error: ' + e));
        };
        
        const deleteFile = (filename) => {
            if (!confirm('Delete ' + filename + '?')) return;
            
            fetch('/delete-file', {
                method: 'POST',
                headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
                body: 'path=' + encodeURIComponent(filename)
            })
            .then(() => getFileList())
            .catch((e) => alert('Delete failed: ' + e));
        };
        
        const httpUpload = () => {
            const fileSelector = document.getElementById('file-selector');
            const file = fileSelector.files[0];
            
            if (!file) {
                alert('Please select a file');
                return;
            }
            
            const formData = new FormData();
            formData.append('file', file);
            
            // Create progress UI
            const progressContainer = document.createElement('div');
            progressContainer.style.cssText = 'position:fixed;background-color:rgb(219,219,213);padding:12px;border:6px double rgb(139,139,139);z-index:1000;min-width:400px;font-family:monospace';
            
            // Center using JS to avoid fractional pixels from transform
            const containerWidth = 424; // 400 + 12*2
            const containerHeight = 120; // approximate
            progressContainer.style.left = Math.round((window.innerWidth - containerWidth) / 2) + 'px';
            progressContainer.style.top = Math.round((window.innerHeight - containerHeight) / 2) + 'px';
            
            const progressTitle = document.createElement('div');
            progressTitle.textContent = 'Uploading: ' + file.name;
            progressTitle.style.cssText = 'margin-bottom:12px;font-weight:bold;color:rgb(26,26,26)';
            
            const progressBarBg = document.createElement('div');
            progressBarBg.style.cssText = 'height:24px;border:2px inset rgb(237,237,237);background-color:white;margin-bottom:8px';
            
            const progressBar = document.createElement('div');
            progressBar.style.cssText = 'height:100%;background-color:rgb(0,0,170);width:0%;transition:width 0.1s';
            
            const progressText = document.createElement('div');
            progressText.textContent = '0 / ' + (file.size / 1024).toFixed(1) + ' KB (0%)';
            progressText.style.cssText = 'font-size:14px;color:rgb(26,26,26);text-align:center';
            
            progressBarBg.appendChild(progressBar);
            progressContainer.appendChild(progressTitle);
            progressContainer.appendChild(progressBarBg);
            progressContainer.appendChild(progressText);
            document.body.appendChild(progressContainer);
            
            // Use XMLHttpRequest for upload progress tracking
            const xhr = new XMLHttpRequest();
            
            xhr.upload.addEventListener('progress', (e) => {
                if (e.lengthComputable) {
                    const percentComplete = Math.round((e.loaded / e.total) * 100);
                    progressBar.style.width = percentComplete + '%';
                    progressText.textContent = (e.loaded / 1024).toFixed(1) + ' / ' + (e.total / 1024).toFixed(1) + ' KB (' + percentComplete + '%)';
                }
            });
            
            xhr.addEventListener('load', () => {
                if (xhr.status === 200) {
                    progressTitle.textContent = 'Upload Complete: ' + file.name;
                    progressBar.style.width = '100%';
                    setTimeout(() => {
                        document.body.removeChild(progressContainer);
                        fileSelector.value = '';
                        getFileList();
                    }, 1500);
                } else {
                    progressTitle.textContent = 'Upload Failed: ' + file.name;
                    progressText.textContent = 'Error: ' + xhr.status;
                    progressBar.style.backgroundColor = 'rgb(170,0,0)';
                    setTimeout(() => {
                        document.body.removeChild(progressContainer);
                    }, 3000);
                }
            });
            
            xhr.addEventListener('error', () => {
                progressTitle.textContent = 'Upload Error: ' + file.name;
                progressText.textContent = 'Network error occurred';
                progressBar.style.backgroundColor = 'rgb(170,0,0)';
                setTimeout(() => {
                    document.body.removeChild(progressContainer);
                }, 3000);
            });
            
            xhr.open('POST', '/upload-file?path=' + encodeURIComponent(currentPath));
            xhr.send(formData);
        };
        
        Promise.all([
            getSettings(),
            getSpeedDial(),
            getFileList()
        ]).catch((e) => {
            console.error(e);
        });
    </script>
</body>
</html>
)rawliteral";

WebServer::WebServer()
    : server(nullptr), events(nullptr), lastStatusUpdate(0) {}

void WebServer::begin() {
  if (server != nullptr) {
    return; // Already initialized
  }

  server = new AsyncWebServer(80);
  events = new AsyncEventSource("/events");

  events->onConnect(
      [this](AsyncEventSourceClient *client) { sendStatusUpdate(); });

  server->addHandler(events);
  setupRoutes();
  server->begin();
}

void WebServer::loop() {
  // Send status updates every 2 seconds
  if (millis() - lastStatusUpdate > 2000) {
    sendStatusUpdate();
    lastStatusUpdate = millis();
  }
}

void WebServer::setupRoutes() {
  // Main page
  server->on("/", HTTP_GET,
             [this](AsyncWebServerRequest *request) { handleRoot(request); });

  // Status endpoints
  server->on("/get-status", HTTP_GET, [this](AsyncWebServerRequest *request) {
    handleGetStatus(request);
  });

  server->on("/get-settings", HTTP_GET, [this](AsyncWebServerRequest *request) {
    handleGetSettings(request);
  });

  server->on(
      "/get-speeddials", HTTP_GET,
      [this](AsyncWebServerRequest *request) { handleGetSpeedDials(request); });

  // Control endpoints
  server->on(
      "/dial", HTTP_POST,
      [this](AsyncWebServerRequest *request) { handleDial(request); }, NULL,
      [](AsyncWebServerRequest *request, uint8_t *data, size_t len,
         size_t index, size_t total) {
        // Body handler - required for POST form data parsing
      });

  server->on("/ath", HTTP_POST,
             [this](AsyncWebServerRequest *request) { handleHangup(request); });

  // Settings endpoints
  server->on(
      "/save-settings", HTTP_POST,
      [this](AsyncWebServerRequest *request) { handleSaveSettings(request); },
      NULL,
      [](AsyncWebServerRequest *request, uint8_t *data, size_t len,
         size_t index, size_t total) {
        // Body handler - required for POST form data parsing
      });

  server->on("/reload-settings", HTTP_POST,
             [this](AsyncWebServerRequest *request) {
               handleReloadSettings(request);
             });

  server->on(
      "/update-speeddial", HTTP_POST,
      [this](AsyncWebServerRequest *request) {
        handleUpdateSpeedDial(request);
      },
      NULL,
      [](AsyncWebServerRequest *request, uint8_t *data, size_t len,
         size_t index, size_t total) {
        // Body handler - required for POST form data parsing
      });

  server->on("/factory-defaults", HTTP_POST,
             [this](AsyncWebServerRequest *request) {
               handleFactoryDefaults(request);
             });

  // File manager endpoints
  server->on("/list-files", HTTP_GET, [this](AsyncWebServerRequest *request) {
    handleListFiles(request);
  });

  server->on(
      "/download-file", HTTP_GET,
      [this](AsyncWebServerRequest *request) { handleDownloadFile(request); });

  server->on("/delete-file", HTTP_POST, [this](AsyncWebServerRequest *request) {
    handleDeleteFile(request);
  });

  server->on(
      "/create-folder", HTTP_POST,
      [this](AsyncWebServerRequest *request) { handleCreateFolder(request); });

  server->on(
      "/upload-file", HTTP_POST,
      [this](AsyncWebServerRequest *request) { request->send(200); },
      [this](AsyncWebServerRequest *request, String filename, size_t index,
             uint8_t *data, size_t len, bool final) {
        handleUploadFile(request, filename, index, data, len, final);
      });
}

void WebServer::handleRoot(AsyncWebServerRequest *request) {
  AsyncWebServerResponse *response =
      request->beginResponse(200, "text/html", index_html);
  request->send(response);
}

void WebServer::handleGetStatus(AsyncWebServerRequest *request) {
  String json = "{";
  json += "\"wifiStatus\":\"" +
          String(network.isWiFiConnected() ? "CONNECTED" : "DISCONNECTED") +
          "\",";
  json += "\"ssidStatus\":\"" + network.getSSID() + "\",";
  json += "\"macAddress\":\"" + WiFi.macAddress() + "\",";
  json += "\"ipAddress\":\"" + network.getIP() + "\",";
  json += "\"gateway\":\"" + WiFi.gatewayIP().toString() + "\",";
  json += "\"subnet\":\"" + WiFi.subnetMask().toString() + "\",";
  json += "\"baudStatus\":\"" + String(storage.getBaudRate()) + "\",";
  json += "\"build\":\"" + String(VERSION_BUILD) + "\",";
  json += "\"callStatus\":\"" +
          String(modem.isConnected() ? "CONNECTED" : "READY") + "\",";
  json += "\"callLength\":\"" + modem.getConnectionDuration() + "\"";
  json += "}";

  request->send(200, "application/json", json);
}

void WebServer::sendStatusUpdate() {
  if (!events) {
    return;
  }

  if (events->count() == 0) {
    return; // No clients connected
  }

  String json = "{";
  json += "\"wifiStatus\":\"" +
          String(WiFi.status() == WL_CONNECTED ? "CONNECTED" : "DISCONNECTED") +
          "\",";
  json += "\"ssidStatus\":\"" + network.getSSID() + "\",";
  json += "\"macAddress\":\"" + WiFi.macAddress() + "\",";
  json += "\"ipAddress\":\"" + network.getIP() + "\",";
  json += "\"gateway\":\"" + WiFi.gatewayIP().toString() + "\",";
  json += "\"subnet\":\"" + WiFi.subnetMask().toString() + "\",";
  json += "\"baudStatus\":\"" + String(storage.getBaudRate()) + "\",";
  json += "\"build\":\"" + String(VERSION_BUILD) + "\",";
  json += "\"callStatus\":\"" +
          String(modem.isConnected() ? "CONNECTED" : "READY") + "\",";
  json += "\"callLength\":\"" + modem.getConnectionDuration() + "\"";
  json += "}";

  events->send(json.c_str(), "status", millis());
}

void WebServer::handleGetSettings(AsyncWebServerRequest *request) {
  int baudIndex = 4; // Default 9600
  int baudRate = storage.getBaudRate();
  if (baudRate == 300)
    baudIndex = 0;
  else if (baudRate == 1200)
    baudIndex = 1;
  else if (baudRate == 2400)
    baudIndex = 2;
  else if (baudRate == 4800)
    baudIndex = 3;
  else if (baudRate == 9600)
    baudIndex = 4;
  else if (baudRate == 19200)
    baudIndex = 5;
  else if (baudRate == 38400)
    baudIndex = 6;
  else if (baudRate == 57600)
    baudIndex = 7;
  else if (baudRate == 115200)
    baudIndex = 8;

  String json = "{";
  json += "\"serialspeed\":" + String(baudIndex) + ",";
  json += "\"echo\":" + String(storage.getEcho() ? 1 : 0) + ",";
  json += "\"quietMode\":" + String(storage.getQuiet() ? 1 : 0) + ",";
  json += "\"verboseResults\":" + String(storage.getVerbose() ? 1 : 0) + ",";
  json += "\"telnet\":" + String(storage.getTelnet() ? 1 : 0) + ",";
  json += "\"autoAnswer\":" + String(storage.getAutoAnswer() ? 1 : 0) + ",";
  json += "\"ftpEnabled\":" + String(storage.getFtpEnabled() ? 1 : 0) + ",";
  json += "\"serverPort\":" + String(storage.getServerPort()) + ",";
  json += "\"busyMsg\":\"" + storage.getBusyMsg() + "\",";
  json += "\"buzzer\":" + String(storage.getBuzzerEnabled() ? 1 : 0);
  json += "}";

  request->send(200, "application/json", json);
}

void WebServer::handleGetSpeedDials(AsyncWebServerRequest *request) {
  String json = "[";
  for (int i = 0; i < 10; i++) {
    if (i > 0)
      json += ",";
    json += "\"" + storage.getSpeedDial(i) + "\"";
  }
  json += "]";

  request->send(200, "application/json", json);
}

void WebServer::handleSaveSettings(AsyncWebServerRequest *request) {
  // All form fields are always sent, so read them directly

  // Update baud rate
  int baudIndex = request->getParam("baud", true)->value().toInt();
  int baudRates[] = {300, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200};
  if (baudIndex >= 0 && baudIndex < 9) {
    storage.setBaudRate(baudRates[baudIndex]);
  }

  // Update server port
  int serverPort = request->getParam("serverport", true)->value().toInt();
  if (serverPort >= 0 && serverPort <= 65535) {
    storage.setServerPort(serverPort);
  }

  // Update busy message
  String busyMsg = request->getParam("busymsg", true)->value();
  busyMsg.trim();
  if (busyMsg.length() > 0 && busyMsg.length() <= 100) {
    storage.setBusyMsg(busyMsg);
  }

  // Update boolean settings - HTML checkboxes only send parameter when checked
  // Checked checkbox sends "echo=1", unchecked sends nothing
  storage.setEcho(request->hasParam("echo", true));
  storage.setQuiet(request->hasParam("quiet", true));
  storage.setVerbose(request->hasParam("verbose", true));
  storage.setTelnet(request->hasParam("telnet", true));
  storage.setAutoAnswer(request->hasParam("autoanswer", true));
  ftpServer.setEnabled(request->hasParam("ftp", true));
  storage.setBuzzerEnabled(request->hasParam("buzzer", true));

  request->redirect("/");
}

void WebServer::handleReloadSettings(AsyncWebServerRequest *request) {
  storage.loadSettings();
  request->redirect("/");
}

void WebServer::handleDial(AsyncWebServerRequest *request) {
  if (request->hasParam("address", true)) {
    String address = request->getParam("address", true)->value();
    modem.dialTo(address);
  }
  request->redirect("/");
}

void WebServer::handleHangup(AsyncWebServerRequest *request) {
  modem.hangUp();
  request->redirect("/");
}

void WebServer::handleUpdateSpeedDial(AsyncWebServerRequest *request) {
  for (int i = 0; i < 10; i++) {
    String paramName = String(i);
    if (request->hasParam(paramName, true)) {
      storage.setSpeedDial(i, request->getParam(paramName, true)->value());
    }
  }
  request->redirect("/");
}

void WebServer::handleListFiles(AsyncWebServerRequest *request) {
  // SD should already be initialized by lilka::begin()
  // Don't call SD.begin() again - it causes mount failures
  String path = "/";
  if (request->hasParam("path")) {
    path = request->getParam("path")->value();
  }

  // Security: prevent directory traversal
  if (path.indexOf("..") >= 0) {
    request->send(403, "application/json", "{\"error\":\"Invalid path\"}");
    return;
  }

  // Ensure path starts with /
  if (!path.startsWith("/")) {
    path = "/" + path;
  }

  String json = "{\"files\":[";
  File root = SD.open(path);
  bool first = true;

  if (!root) {
    // SD not ready yet or path doesn't exist
    request->send(500, "application/json",
                  "{\"error\":\"SD card not ready or path not found\"}");
    return;
  }

  if (root.isDirectory()) {
    File file = root.openNextFile();
    while (file) {
      if (!first)
        json += ",";

      String fullPath = path;
      if (!fullPath.endsWith("/"))
        fullPath += "/";
      fullPath += String(file.name());

      json += "{\"name\":\"";
      json += String(file.name());
      json += "\",\"path\":\"";
      json += fullPath;
      json += "\",\"isDir\":";
      json += file.isDirectory() ? "true" : "false";

      if (!file.isDirectory()) {
        json += ",\"size\":";
        json += String(file.size());
      }

      json += "}";
      first = false;

      file.close();
      file = root.openNextFile();
    }
    root.close();
  }

  json += "],\"used\":";
  json += String(SD.usedBytes());
  json += ",\"total\":";
  json += String(SD.totalBytes());
  json += "}";

  request->send(200, "application/json", json);
}

void WebServer::handleDownloadFile(AsyncWebServerRequest *request) {
  if (!request->hasParam("path")) {
    request->send(400, "text/plain", "Missing path parameter");
    return;
  }

  String path = request->getParam("path")->value();

  // Security: prevent directory traversal
  if (path.indexOf("..") >= 0) {
    request->send(403, "text/plain", "Invalid path");
    return;
  }

  // Ensure path starts with /
  if (!path.startsWith("/")) {
    path = "/" + path;
  }

  if (!SD.exists(path)) {
    request->send(404, "text/plain", "File not found");
    return;
  }

  request->send(SD, path, "application/octet-stream", true);
}

void WebServer::handleDeleteFile(AsyncWebServerRequest *request) {
  if (!request->hasParam("path", true)) {
    request->send(400, "text/plain", "Missing path parameter");
    return;
  }

  String path = request->getParam("path", true)->value();

  // Security: prevent directory traversal
  if (path.indexOf("..") >= 0) {
    request->send(403, "text/plain", "Invalid path");
    return;
  }

  // Ensure path starts with /
  if (!path.startsWith("/")) {
    path = "/" + path;
  }

  // Check if it's a directory
  File entry = SD.open(path);
  if (!entry) {
    request->send(404, "text/plain", "Path not found");
    return;
  }

  bool isDir = entry.isDirectory();
  entry.close();

  if (isDir) {
    // Recursively delete directory
    if (deleteDirectory(path)) {
      request->send(200, "text/plain", "Folder deleted");
    } else {
      request->send(500, "text/plain", "Failed to delete folder");
    }
  } else {
    // Delete file
    if (SD.remove(path)) {
      request->send(200, "text/plain", "File deleted");
    } else {
      request->send(500, "text/plain", "Failed to delete file");
    }
  }
}

bool WebServer::deleteDirectory(const String &path) {
  File dir = SD.open(path);
  if (!dir || !dir.isDirectory()) {
    return false;
  }

  // Delete all files and subdirectories
  File file = dir.openNextFile();
  while (file) {
    String filePath = path;
    if (!filePath.endsWith("/"))
      filePath += "/";
    filePath += String(file.name());

    if (file.isDirectory()) {
      file.close();
      // Recursively delete subdirectory
      if (!deleteDirectory(filePath)) {
        dir.close();
        return false;
      }
    } else {
      file.close();
      // Delete file
      if (!SD.remove(filePath)) {
        dir.close();
        return false;
      }
    }

    file = dir.openNextFile();
  }

  dir.close();

  // Delete the directory itself
  return SD.rmdir(path);
}

void WebServer::handleCreateFolder(AsyncWebServerRequest *request) {
  if (!request->hasParam("path", true) || !request->hasParam("name", true)) {
    request->send(400, "text/plain", "Missing parameters");
    return;
  }

  String path = request->getParam("path", true)->value();
  String name = request->getParam("name", true)->value();

  // Security: prevent directory traversal
  if (path.indexOf("..") >= 0 || name.indexOf("..") >= 0 ||
      name.indexOf("/") >= 0) {
    request->send(403, "text/plain", "Invalid path or name");
    return;
  }

  // Ensure path starts with /
  if (!path.startsWith("/")) {
    path = "/" + path;
  }

  // Build full folder path
  String folderPath = path;
  if (!folderPath.endsWith("/")) {
    folderPath += "/";
  }
  folderPath += name;

  // Create directory
  if (SD.mkdir(folderPath)) {
    request->send(200, "text/plain", "Folder created");
  } else {
    request->send(500, "text/plain", "Failed to create folder");
  }
}

static File uploadFile;
static String uploadTargetPath;

void WebServer::handleUploadFile(AsyncWebServerRequest *request,
                                 String filename, size_t index, uint8_t *data,
                                 size_t len, bool final) {
  if (index == 0) {
    // Get target directory from query parameter
    String targetDir = "/";
    if (request->hasParam("path")) {
      targetDir = request->getParam("path")->value();
    }

    // Security: prevent directory traversal
    if (filename.indexOf("..") >= 0 || targetDir.indexOf("..") >= 0) {
      return;
    }

    // Remove leading / from filename
    if (filename.startsWith("/")) {
      filename = filename.substring(1);
    }

    // Ensure targetDir starts with /
    if (!targetDir.startsWith("/")) {
      targetDir = "/" + targetDir;
    }

    // Build full path
    if (targetDir.endsWith("/")) {
      uploadTargetPath = targetDir + filename;
    } else {
      uploadTargetPath = targetDir + "/" + filename;
    }

    // Delete existing file if it exists
    if (SD.exists(uploadTargetPath)) {
      SD.remove(uploadTargetPath);
    }

    // Open file - use FILE_WRITE mode which creates or overwrites
    uploadFile = SD.open(uploadTargetPath, FILE_WRITE);
    if (!uploadFile) {
      // Log error for debugging
      Serial.printf("Failed to open file for writing: %s\n",
                    uploadTargetPath.c_str());
      return;
    }
    Serial.printf("Upload started: %s\n", uploadTargetPath.c_str());
  }

  if (uploadFile && len) {
    size_t written = uploadFile.write(data, len);
    if (written != len) {
      Serial.printf("Write error: wrote %d of %d bytes\n", written, len);
    }
  }

  if (final) {
    if (uploadFile) {
      uploadFile.close();
      Serial.printf("Upload completed: %s\n", uploadTargetPath.c_str());
    }
  }
}

void WebServer::handleFactoryDefaults(AsyncWebServerRequest *request) {
  // Clear settings first
  storage.resetToDefaults();

  // Send response before disconnecting WiFi
  request->send(
      200, "text/plain",
      "Factory defaults restored. WiFi will disconnect in 2 seconds.");

  // Schedule WiFi disconnect after response is fully transmitted
  // Using a longer delay to ensure the HTTP response completes
  static bool resetScheduled = false;
  if (!resetScheduled) {
    resetScheduled = true;
    // This lambda will execute after we return from this handler
    request->onDisconnect([]() {
      delay(1000);
      WiFi.disconnect(true, true);
      displayUI.setWiFiStatus(false, "", "");
    });
  }
}
