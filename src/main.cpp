#include "main.hpp"
#include "screen.hpp"

#include <coreinit/memory.h>

#include <nsyshid/hid.h>

#include <vpad/input.h>

#include <whb/log.h>
#include <whb/log_udp.h>
#include <whb/proc.h>

VPADStatus vpad;
VPADReadError err;

HIDClient hidc;
HIDDevice* device[8];
uint8_t* device_mem[8];

int devIndex = 0;
int menu = 0;

uint8_t* readMem;
int memStatus = 0;

uint8_t* writeMem;
int writePos = 0;
int wstatus = 0;

#define BUFFER_SIZE_MAX 0x100

uint32_t readFrames = 0;

void writeCallback(uint32_t handle, int32_t error, uint8_t *buffer, uint32_t bytesTransferred, void* userContext) {
    WHBLogPrintf("Callback write of device. Handle: %x, err: %d, size: %x", handle, err, bytesTransferred);
}

void readCallback(uint32_t handle, int32_t error, uint8_t *buffer, uint32_t bytesTransferred, void* userContext) {
    //WHBLogPrintf("bt: %x, err: %d, handle: %x", bytesTransferred, error, handle);

    if (!error){
        if (device[devIndex] != nullptr && device[devIndex]->handle == handle) {
            if (error < 0) {
                memStatus = error;
                WHBLogPrintf("HID Read error: %d", error);
                return;
            }
            else {
                memStatus = bytesTransferred;
                OSBlockMove(readMem, buffer, bytesTransferred, false);
            }
        }

        HIDRead(handle, buffer, bytesTransferred, readCallback, NULL);
    }
    else
        WHBLogPrintf("HID Read error: %d", error);
}

int attachCallback(HIDClient* cl, HIDDevice *dev, HIDAttachEvent event) {
    if (event){
        int index = -1;
        for (int i = 0; i < 8; i++){
            if (device[i] == nullptr){
                index = i;
                device[i] = dev;
                break;
            }
        }
        if (index < 0){
            WHBLogPrint("New device cannot be handled (max 8 devices reached)");
        }

        WHBLogPrintf("");
        WHBLogPrintf("Device attached");
        WHBLogPrintf("dev->handle: %08x", dev->handle);
        WHBLogPrintf("dev->physicalDeviceInst: %08x", dev->physicalDeviceInst);
        WHBLogPrintf("dev->vid: %04x", dev->vid);
        WHBLogPrintf("dev->pid: %04x", dev->pid);
        WHBLogPrintf("dev->interfaceIndex: %02x", dev->interfaceIndex);
        WHBLogPrintf("dev->subClass: %02x", dev->subClass);
        WHBLogPrintf("dev->protocol: %02x", dev->protocol);
        WHBLogPrintf("dev->maxPacketSizeRx: %04x", dev->maxPacketSizeRx);
        WHBLogPrintf("dev->maxPacketSizeTx: %04x", dev->maxPacketSizeTx);

        if (index >= 0){
            HIDRead(dev->handle, device_mem[index], dev->maxPacketSizeRx, readCallback, NULL);
        }

        return 1;
    }
    else{
        WHBLogPrintf("");
        WHBLogPrint("Device detached");
        for (int i = 0; i < 8; i++){
            if (device[i] == dev){
                device[i] = nullptr;
                break;
            }
        }
    }
    return 0;
}

int main() {
    WHBLogUdpInit();
    WHBProcInit();
    
    WHBLogPrintf("Initialising screen...");

    initScreen();
    clearBuffers();

    readMem = (uint8_t*)MEMAllocFromDefaultHeap(0x100);
    writeMem = (uint8_t*)MEMAllocFromDefaultHeap(0x100);

    for (int i = 0; i < 8; i++){
        device_mem[i] = (uint8_t*)MEMAllocFromDefaultHeap(0x100);
    }

    WHBLogPrintf("Initialising HID...");
    int setup = HIDSetup();
    WHBLogPrintf("HIDSetup: %08x", setup);
    HIDAddClient(&hidc, attachCallback);
    render();

    int col = 0;
    while (WHBProcIsRunning()){
        VPADRead(VPAD_CHAN_0, &vpad, 1, &err);
        
        if (vpad.trigger & VPAD_BUTTON_L){
            if (devIndex < 1)
                devIndex = 7;
            else
                devIndex--;
        }
        else if (vpad.trigger & VPAD_BUTTON_R){
            if (devIndex > 6)
                devIndex = 0;
            else
                devIndex++;
        }

        if (vpad.trigger & VPAD_BUTTON_PLUS){
            if (menu == 1)
                menu = 0;
            else {
                menu = 1;
                readFrames = 0;
            }
        }
        else if (vpad.trigger & VPAD_BUTTON_MINUS){
            if (menu == 2)
                menu = 0;
            else {
                menu = 2;
                writePos = 0;
            }
        }

        clearBuffers();
        writef(50, 0, "Device %d/8", devIndex + 1);
        if (device[devIndex] != nullptr){
            switch(menu){
                case 0:
                    writef(0, col++, "dev->handle: %08x", device[devIndex]->handle);
                    writef(0, col++, "dev->physicalDeviceInst: %08x", device[devIndex]->physicalDeviceInst);
                    writef(0, col++, "dev->vid: %04x", device[devIndex]->vid);
                    writef(0, col++, "dev->pid: %04x", device[devIndex]->pid);
                    writef(0, col++, "dev->interfaceIndex: %02x", device[devIndex]->interfaceIndex);
                    writef(0, col++, "dev->subClass: %02x", device[devIndex]->subClass);
                    writef(0, col++, "dev->protocol: %02x", device[devIndex]->protocol);
                    writef(0, col++, "dev->maxPacketSizeRx: %04x", device[devIndex]->maxPacketSizeRx);
                    writef(0, col++, "dev->maxPacketSizeTx: %04x", device[devIndex]->maxPacketSizeTx);

                    write(0, ++col, "Press L/R to change device index");
                    write(0, ++col, "Press PLUS to read memory from this device");
                    write(0, ++col, "Press MINUS to write data to this device");
                    col = 0;

                    write(0, 17, "WiiU HID Tester by Pokes303");
                    break;
                case 1:
                    writef(0, 0, "vid: %04x, pid: %04x", device[devIndex]->vid, device[devIndex]->pid);

                    if (memStatus >= 0){
                        writef(25, 0, "b: %x, f: %u", memStatus, ++readFrames);

                        for (int i = 0; i < memStatus; i++){
                            writef((i % 16) * 3, (i / 16) + 2, "%02x", readMem[i]);
                        }
                    }
                    else{
                        writef(25, 0, "error: %d", memStatus);
                    }
                    break;
                case 2:
                        if (vpad.trigger & VPAD_BUTTON_A){
                            wstatus = HIDWrite(device[devIndex]->handle, writeMem, BUFFER_SIZE_MAX, writeCallback, NULL);
                            OSBlockSet(writeMem, 0, BUFFER_SIZE_MAX);

                            WHBLogPrintf("HIDWrite(): %d", wstatus);
                        }
                        else if (vpad.trigger & VPAD_STICK_R_EMULATION_UP && writePos - 16 >= 0){
                            writePos -= 16;
                        }
                        else if (vpad.trigger & VPAD_STICK_R_EMULATION_DOWN && writePos + 16 < BUFFER_SIZE_MAX){
                            writePos += 16;
                        }
                        else if (vpad.trigger & VPAD_STICK_R_EMULATION_RIGHT && writePos < BUFFER_SIZE_MAX - 1){
                            writePos++;
                        }
                        else if (vpad.trigger & VPAD_STICK_R_EMULATION_LEFT && writePos > 0){
                            writePos--;
                        }
                        else if (vpad.trigger & VPAD_BUTTON_UP || vpad.hold & VPAD_STICK_L_EMULATION_UP){
                            writeMem[writePos]++;
                        }
                        else if (vpad.trigger & VPAD_BUTTON_DOWN || vpad.hold & VPAD_STICK_L_EMULATION_DOWN){
                            writeMem[writePos]--;
                        }

                        for (int i = 0; i < 0x100; i++){
                            writef((i % 16) * 3, i / 16, "%02x", writeMem[i]);
                        }

                        writef((writePos % 16) * 3 - 1, writePos / 16, ">  <");
                        write(50, 2, "Press A to send");
                        write(50, 3, "Last write status:");
                        writef(50, 4, "%d", wstatus);
                    break;
            }
        }
        else {
            writef(0, 0, "No device attached at index %d", devIndex + 1);
        }
        render();
    }

    HIDDelClient(&hidc);
    HIDTeardown();

    for (int i = 0; i < 8; i++){
        MEMFreeToDefaultHeap(device_mem[i]);
    }

    MEMFreeToDefaultHeap(readMem);
    MEMFreeToDefaultHeap(writeMem);

    shutdownScreen();

    WHBProcShutdown();
    WHBLogUdpDeinit();

    return 0;
}