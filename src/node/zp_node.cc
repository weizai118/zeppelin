// Copyright 2017 Qihoo
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http:// www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <getopt.h>
#include <glog/logging.h>
#include <iostream>
#include <sstream>

#include "slash/include/env.h"
#include "src/node/zp_data_server.h"
#include "include/zp_conf.h"


ZpConf *g_zp_conf;
ZPDataServer* zp_data_server;

void Usage();

static void GlogInit() {
  if (!slash::FileExists(g_zp_conf->log_path())) {
    slash::CreatePath(g_zp_conf->log_path());
  }

  FLAGS_alsologtostderr = true;

  FLAGS_log_dir = g_zp_conf->log_path();
  FLAGS_minloglevel = 0;
  FLAGS_max_log_size = 1800;
  // @TODO rm
  FLAGS_logbufsecs = 0;

  ::google::InitGoogleLogging("zp");
}


static void IntSigHandle(const int sig) {
  LOG(INFO) << "Catch Signal " << sig << ", cleanup...";
  zp_data_server->Exit();
  LOG(INFO) << "data server Exit";
}

static void ZPDataSignalSetup() {
  signal(SIGHUP, SIG_IGN);
  signal(SIGPIPE, SIG_IGN);
  signal(SIGINT, &IntSigHandle);
  signal(SIGQUIT, &IntSigHandle);
  signal(SIGTERM, &IntSigHandle);
}


void Usage() {
  printf("Usage:\n"
          "  ./zp-node -c conf_file\n"
          "  ./zp-node -v\n");
}

void ZpConfInit(int argc, char* argv[]) {
  if (argc < 1) {
    Usage();
    exit(-1);
  }
  bool path_opt = false;
  char c;
  char path[1024];
  while (-1 != (c = getopt(argc, argv, "c:hv"))) {
    switch (c) {
      case 'c':
        snprintf(path, sizeof(path), "%s", optarg);
        path_opt = true;
        break;
      case 'h':
        Usage();
        exit(-1);
      case 'v':
        std::cout << "Zeppelin " << std::endl;
        std::cout << "Git ver: " << kZPVersion << std::endl;
        std::cout << "Date:    " << kZPCompileDate << std::endl;
        exit(0);
      default:
        Usage();
        exit(-1);
    }
  }

  if (path_opt == false) {
    fprintf(stderr, "Please specify the conf file path\n");
    Usage();
    exit(-1);
  }

  g_zp_conf = new ZpConf();
  if (g_zp_conf->Load(path) != 0) {
    LOG(FATAL) << "zp-meta load conf file error";
  }
  g_zp_conf->Dump();
}

int main(int argc, char** argv) {
  ZpConfInit(argc, argv);
  if (g_zp_conf->daemonize()) {
    daemonize();
  }

  GlogInit();
  ZPDataSignalSetup();

  FileLocker db_lock(g_zp_conf->lock_file());
  Status s = db_lock.Lock();
  if (!s.ok()) {
    printf("Start ZPDataServer failed, because LOCK file error\n");
    return 1;
  }

  if (g_zp_conf->daemonize()) {
    create_pid_file();
    close_std();
  }

  zp_data_server = new ZPDataServer();

  zp_data_server->Start();

  //  printf ("Exit\n");
  delete zp_data_server;

  ::google::ShutdownGoogleLogging();
  return 0;
}

