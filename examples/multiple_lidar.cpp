/*********************************************************************
* Software License Agreement (BSD License)
*
*  Copyright (c) 2018, EAIBOT, Inc.
*  All rights reserved.
*
*  Redistribution and use in source and binary forms, with or without
*  modification, are permitted provided that the following conditions
*  are met:
*
*   * Redistributions of source code must retain the above copyright
*     notice, this list of conditions and the following disclaimer.
*   * Redistributions in binary form must reproduce the above
*     copyright notice, this list of conditions and the following
*     disclaimer in the documentation and/or other materials provided
*     with the distribution.
*   * Neither the name of the Willow Garage nor the names of its
*     contributors may be used to endorse or promote products derived
*     from this software without specific prior written permission.
*
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
*  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
*  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
*  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
*  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
*  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
*  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
*  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
*  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
*  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
*  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
*  POSSIBILITY OF SUCH DAMAGE.
*********************************************************************/

#include <iostream>
#include <string>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <vector>
#include <ctime>
#include <cmath>
#include <core/base/timer.h>
#include "CYdLidar.h"
#include "core/common/ydlidar_help.h"
#include "core/common/ydlidar_protocol.h"
#include "filters/StrongLightFilter.h"


using namespace std;
using namespace ydlidar;

#if defined(_MSC_VER)
#pragma comment(lib, "ydlidar_sdk.lib")
#endif

/**
 * @brief gs test
 * @param argc
 * @param argv
 * @return
 * @par Flow chart
 * Step1: instance CYdLidar.\n
 * Step2: set paramters.\n
 * Step3: initialize SDK and LiDAR.(::CYdLidar::initialize)\n
 * Step4: Start the device scanning routine which runs on a separate thread and enable motor.(::CYdLidar::turnOn)\n
 * Step5: Get the LiDAR Scan Data.(::CYdLidar::doProcessSimple)\n
 * Step6: Stop the device scanning thread and disable motor.(::CYdLidar::turnOff)\n
 * Step7: Uninitialize the SDK and Disconnect the LiDAR.(::CYdLidar::disconnecting)\n
 */

// 2024110800141338

int main(int argc, char *argv[]) {
  printf("__   ______  _     ___ ____    _    ____  \n");
  printf("\\ \\ / /  _ \\| |   |_ _|  _ \\  / \\  |  _ \\ \n");
  printf(" \\ V /| | | | |    | || | | |/ _ \\ | |_) | \n");
  printf("  | | | |_| | |___ | || |_| / ___ \\|  _ <  \n");
  printf("  |_| |____/|_____|___|____/_/   \\_\\_| \\_\\ \n");
  printf("\n");
  fflush(stdout);
  std::string port;
  ydlidar::os_init();

  std::map<std::string, std::string> ports = ydlidar::lidarPortList();
  std::map<std::string, std::string>::iterator it;

  if (argc < 2) { // if we have one id
      return -1;
  }
  std::cout << argv[1] << std::endl;
  std::string string_to_check = argv[1];
  int baudrate = 921600; // the baudrate is fixed now

  if (!ydlidar::os_isOk()) {
    return -1;
  }

  bool isSingleChannel = false;
  float frequency = 8.0;
  /// ignore array
  std::string ignore_array;
  bool ret = true;
  int id = 0;
  CYdLidar laserGlobal;
  for (it = ports.begin(); it != ports.end(); it++) { // we loop in all ports to find the lidar we want
    std::cout << "############ looping in devices ###############" <<std::endl;
    CYdLidar laserLoop;
    if(laserLoop.isScanning()){
      std::cout << "Already in use" << std::endl;
      continue;// if the LIDAR is already used, jump to next iteration
    }    
    printf("[%d] %s %s\n", id, it->first.c_str(), it->second.c_str());
    port = it->second;
    id++;
    if (it->second.find("USB") != std::string::npos) { // Lidars are only on USBs ports
      laserLoop.setlidaropt(LidarPropSerialPort, port.c_str(), port.size()); // setting the port
      //////////////////////int property/////////////////
      ignore_array.clear();
      laserLoop.setlidaropt(LidarPropIgnoreArray, ignore_array.c_str(),
                        ignore_array.size());
      /// lidar baudrate
      laserLoop.setlidaropt(LidarPropSerialBaudrate, &baudrate, sizeof(int));
      /// gs lidar
      int optval = TYPE_GS;
      laserLoop.setlidaropt(LidarPropLidarType, &optval, sizeof(int));
      /// device type (YDLIDAR_TYPE_TCP,YDLIDAR_TYPE_SERIAL)
      optval = YDLIDAR_TYPE_SERIAL; 
      laserLoop.setlidaropt(LidarPropDeviceType, &optval, sizeof(int));
      /// sample rate
      optval = isSingleChannel ? 3 : 4;
      laserLoop.setlidaropt(LidarPropSampleRate, &optval, sizeof(int));
      /// abnormal count
      optval = 4;
      laserLoop.setlidaropt(LidarPropAbnormalCheckCount, &optval, sizeof(int));
      /// Intenstiy bit count
      optval = 8;
      laserLoop.setlidaropt(LidarPropIntenstiyBit, &optval, sizeof(int));

      //////////////////////bool property/////////////////
      /// fixed angle resolution
      bool b_optvalue = false;
      laserLoop.setlidaropt(LidarPropFixedResolution, &b_optvalue, sizeof(bool));
      /// rotate 180
      laserLoop.setlidaropt(LidarPropReversion, &b_optvalue, sizeof(bool));
      /// Counterclockwise
      laserLoop.setlidaropt(LidarPropInverted, &b_optvalue, sizeof(bool));
      b_optvalue = true;
      laserLoop.setlidaropt(LidarPropAutoReconnect, &b_optvalue, sizeof(bool));
      /// one-way communication
      laserLoop.setlidaropt(LidarPropSingleChannel, &isSingleChannel, sizeof(bool));
      /// intensity
      b_optvalue = true;
      laserLoop.setlidaropt(LidarPropIntenstiy, &b_optvalue, sizeof(bool));
      /// Motor DTR
      b_optvalue = true;
      laserLoop.setlidaropt(LidarPropSupportMotorDtrCtrl, &b_optvalue, sizeof(bool));
      /// HeartBeat
      b_optvalue = false;
      laserLoop.setlidaropt(LidarPropSupportHeartBeat, &b_optvalue, sizeof(bool));

      //////////////////////float property/////////////////
      // unit: °
      float f_optvalue = 180.0f;
      laserLoop.setlidaropt(LidarPropMaxAngle, &f_optvalue, sizeof(float));
      f_optvalue = -180.0f;
      laserLoop.setlidaropt(LidarPropMinAngle, &f_optvalue, sizeof(float));
      // unit: m
      f_optvalue = 1.f;
      laserLoop.setlidaropt(LidarPropMaxRange, &f_optvalue, sizeof(float));
      f_optvalue = 0.025f;
      laserLoop.setlidaropt(LidarPropMinRange, &f_optvalue, sizeof(float));
      // unit: Hz
      laserLoop.setlidaropt(LidarPropScanFrequency, &frequency, sizeof(float));
      if (!laserLoop.initialize()) {
          fprintf(stderr, "Fail to initialize %s\n", laserLoop.DescribeError());
          fflush(stderr);
          continue; // if can(t initialize this one, jump to nxt iteration
      }
    } else {
        continue;
    }
    //////////////////////string property/////////////////
    /// lidar port
    std::vector<device_info_ex> disLoop;
    std::string sn;
    std::cout << "before getDeviceInfo" << std::endl;
    laserLoop.getDeviceInfo(disLoop); // retrieving infos of the LIDARD
    // formatting
    if(disLoop.size() == 0){ // When Lidars are busy, they don't answer and DisLoop is then empty
      std::cout << "Disloop size null" << std::endl;
      continue;
    }
    for (int i = 0; i < SDK_SNLEN; i++){ //Stolen from CYDLidar.cpp
      sn += char(disLoop.at(0).di.serialnum[i] + 48);
    }
    std::cout << "first get device info" << std::endl;
    std::cout << sn << std::endl;
    if (sn == string_to_check) {  // if the serial number is correct, it's the one that was setted in the command line
      std::cout << "Correct LIDAR found" << std::endl;
      ret = laserLoop.turnOn();
      std::cout << "turning on laser" << std::endl;
      if (!ret) {
          fprintf(stderr, "Fail to turn on %s\n", laserLoop.DescribeError());
          fflush(stderr);
          continue;
      }

      LaserScan scan;
      StrongLightFilter filter;
      filter.setStrategy(StrongLightFilter::FS_2);
      filter.setMaxDist(0.35);
      std::map<int, uint32_t> ts;
      for (int i=0; i<LIDAR_MAXCOUNT; ++i)
        ts[i] = getms();

      // CSV
      auto now = std::chrono::system_clock::now();
      std::time_t now_c = std::chrono::system_clock::to_time_t(now);
      std::tm now_tm = *std::localtime(&now_c);
      char time_str[100];
      std::strftime(time_str, sizeof(time_str), "%Y%m%d_%H%M%S", &now_tm);
      
      std::string csv_filename = "/tmp/gs_scan_data_" + std::string(time_str) + "_" + sn + ".csv";
      std::ofstream csv_file;
      csv_file.open(csv_filename, std::ios::out | std::ios::trunc);
      bool csv_header_written = false;

      // Starting datalogginf process
      while (ret && ydlidar::os_isOk()) {
        if (laserLoop.doProcessSimple(scan)) {
          //printf("Module [%d] [%d] points in [%.02f]Hz\n",
          // scan.moduleNum,
          // int(scan.points.size()),
          // scan.scanFreq);
          uint32_t t = getms();
          uint32_t dt = t - ts[scan.moduleNum];
          auto now = std::chrono::system_clock::now();
          auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
          std::time_t tt = std::chrono::system_clock::to_time_t(now);
          std::tm tm_local;
    #if defined(_WIN32)
          localtime_s(&tm_local, &tt);
    #else
          localtime_r(&tt, &tm_local);
    #endif
          char time_buf[64] = {0};
          std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", &tm_local);
          // std::cout << "point number: " << scan.points.size() << ", timestamp: " << time_buf << "." << std::setfill('0') << std::setw(3) << (ms % 1000) << std::setfill(' ') << std::endl;
          ts[scan.moduleNum] = t;
          filter.filter(scan, 0, 0, scan);
          if (csv_file.is_open()) {
            csv_file << time_buf << "." << std::setfill('0') << std::setw(3) << (ms % 1000) << std::setfill(' ');
            csv_file << std::fixed;
            for (size_t i = 0; i < scan.points.size(); ++i)
            {
              const LaserPoint &p = scan.points.at(i);
              csv_file << std::setprecision(6) <<  p.angle  << ";" << std::setprecision(6) << p.range << ";";
            }
            csv_file << "\n";
            csv_file.flush();
          }

          for (size_t i = 0; i < scan.points.size(); ++i) {
              const LaserPoint &p = scan.points.at(i);
              float height = p.range * cos(p.angle);
              // printf("%d a %.02f r %.01f h %.01f\n", int(i), 
              // p.angle * 180.0f / M_PI, p.range * 1000.0f, height * 1000.0f);
          }
        } else {
          fprintf(stderr, "APP : Failed to get Lidar Data\n");
          fflush(stderr);
        }
      }

      if (csv_file.is_open()) {
          csv_file.close();
      }

    }else{
      laserLoop.turnOff();
      laserLoop.disconnecting();
    }
  }
  return 0;
}
