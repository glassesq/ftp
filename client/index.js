import { initFTP } from "./ftp.js";
import log4js from "log4js";
import promptSync from "prompt-sync";
import { EventEmitter } from "events";
import { Console, info } from "console";

const prompt = promptSync();

const logger = log4js.getLogger("ftpc");
logger.level = "debug";

const ftpc = {
  ftpc_emitter: new EventEmitter(),
  client: null,
  islogin: true,
  current_action: {
    action:
      "init" /*init, login_u, login_p, rename, cd, pwd, get, send, passive*/,
  },
  info: {
    host: null,
    port: null,
  },
  connectServer: function () {},
  login: function (command, step = "user") {
    if (step == "user") {
      console.log(
        "Try to login to target server " + this.info.host + ":" + this.info.port
      );
      let user;
      if (command.trim() == "login" || command.trim() == "user") {
        user = prompt("Name:");
      } else {
        user = command.trim().split(" ").at(-1);
      }
      this.client.setReact((reaction) => {
        if (reaction.reply.code == 331) {
          console.log(reaction.message);
          this.login(command, "pass");
        } else {
          if (!reaction.wait) {
            console.log(reaction.message);
            this.client.setReact(this.reactOnce.bind(this));
            this.ftpc_emitter.emit("act");
          }
        }
      });
      this.client.runAct({ action: "User", username: user });
    } else if (step == "pass") {
      // console.log("Enter your password.");
      const password = prompt("Password:", { echo: "*" });
      this.client.setReact((reaction) => {
        if (reaction.reply.code == 230) {
          console.log(reaction.message);
          this.login(command, "over");
        } else {
          if (!reaction.wait) {
            console.log(reaction.message);
            this.client.setReact(this.reactOnce.bind(this));
            this.ftpc_emitter.emit("act");
          }
        }
      });
      this.client.runAct({ action: "Pass", password: password });
    } else if (step == "over") {
      this.islogin = true;
      this.client.setReact(this.reactOnce.bind(this));
      this.ftpc_emitter.emit("act");
    }
  },
  rename: function (command) {
    const action = { action: "Rename" };
    if (command.trim() == "mkdir") {
    } else {
      action.from = command.trim().split(" ").at(-2);
      action.to = command.trim().split(" ").at(-1);
    }
    this.client.runAct(action);
  },
  mkdir: function (command) {
    const action = { action: "Mkdir" };
    if (command.trim() == "mkdir") {
    } else {
      action.dir = command.trim().split(" ").at(-1);
    }
    this.client.runAct(action);
  },
  rmdir: function (command) {
    const action = { action: "Rmdir" };
    if (command.trim() == "rmdir") {
    } else {
      action.dir = command.trim().split(" ").at(-1);
    }
    this.client.runAct(action);
  },
  cd: function (command) {
    const action = { action: "Cwd" };
    if (command.trim() == "cd") {
    } else {
      action.dir = command.trim().split(" ").at(-1);
    }
    this.client.runAct(action);
  },
  pwd: function (command) {
    const action = { action: "Pwd" };
    if (command.trim() == "pwd") {
      this.client.runAct(action);
    } else {
      console.log("unknown command");
      this.ftpc_emitter.emit("act");
      return;
    }
  },
  actOnce: function () {
    logger.debug("[actOnce] start listen to user act");
    if (this.current_action.action == "init") {
    }
    /* [event trigger] read from user, shall check context */
    const command = prompt("ftpc>");
    if (command == null) {
      this.client.killService();
      return;
    }
    if (command.trim().length == 0) {
      this.ftpc_emitter.emit("act");
      return;
    }
    // if( command.endsWith("\n") )
    logger.debug("User enter command: " + command);
    /* ret = sendAction to ftpclient */
    if (command.startsWith("login") || command.startsWith("user")) {
      this.login(command, "user");
    } else if (command.startsWith("ls") || command.startsWith("dir")) {
      this.client.runAct({ action: "List" });
    } else if (command.startsWith("mkdir")) {
      this.mkdir(command);
    } else if (command.startsWith("rmdir")) {
      this.rmdir(command);
    } else if (command.startsWith("rename")) {
      this.rename(command);
    } else if (command.startsWith("cd")) {
      this.cd(command);
    } else if (command.startsWith("pwd")) {
      this.client.runAct({ action: "Pwd" });
    } else if (command.startsWith("passive")) {
      this.client.runAct({ action: "Passive" });
    } else if (command.startsWith("system")) {
      this.client.runAct({ action: "System" });
    } else if (command.startsWith("binary")) {
      this.client.runAct({ action: "Binary" });
    } else if (command.startsWith("help")) {
      console.log(this.help_message.join("\n"));
      this.ftpc_emitter.emit("act");
    } else {
      logger.debug("receive a unknown command" + command);
      console.log('invalid command. Try enter "help" to show help page');
      this.ftpc_emitter.emit("act");
    }
  },
  /* display a message from service, decide to wait more info OR actOnce OR kill service*/
  reactOnce: function (reaction) {
    // logger.debug("[reactOnce] gen: " + reply.genMessage());
    logger.debug("[reactOnce] reaction received");
    console.log(reaction.message);
    /* [callback function] get one reply from user */

    /* process reply [with action context]*/
    if (reaction.kill) {
      console.log("FTP service end");
      this.client.killService();
      return;
    }

    if (!reaction.wait) {
      this.ftpc_emitter.emit("act");
      return;
    }

    /* case mark: wait until a new event */
    /* case response and not stop: call actOnce */
    /* case response and stop: exit */
  },
  run: function () {
    this.ftpc_emitter.on("act", this.actOnce.bind(this)); /* act -> actOnce */

    this.client = initFTP();

    console.log("Enter the FTP server host: [default: 0.0.0.0]");
    this.info.host = prompt("ftpc>", "0.0.0.0");
    this.info.host?.replace(" ", "");

    console.log("Enter the FTP server port: [default: 5000]");
    this.info.port = parseInt(prompt("ftpc>", 5000));
    if (!(this.info.port >= 0 && this.info.port <= 65536)) {
      console.log("invalid port");
      return;
    }

    this.client.setAddress(this.info.host, this.info.port);
    this.client.startService();
    this.client.setReact(this.reactOnce.bind(this));
    console.log(
      "Trying to connect to " + this.info.host + ":" + this.info.port
    );

    /* waiting for greeting message */
  },
  help_message: [
    "help: ",
    "binary                      - set to binary mode [TYPE I] ",
    "!bye                         - exit() ",
    "cd <dir>                    - cd directory [CWD] ",
    "dir                         - list current directory [PASV/PORT][LIST] n",
    "exit                        - exit() [QUIT] ",
    "!get <remote-file> <file>    - get remote-file to file [PASV/PORT][RETR] ",
    "help                        - show this help page  ",
    "ls                          - list current directory [PASV/PORT][LIST] ",
    "login                       - Login to the server   [USER/PASS] ",
    "mkdir <dir>                 - make directory  [MKD] ",
    "passive                     - switch PASV/PORT mode   ",
    "pwd                         - show current directory [PWD] ",
    "quit                        - exit() [QUIT] ",
    "rename <file> <new_name>    - rename file [RNFR/RNTO] ",
    "!rmdir <dir>                 - remove directory [RMD] ",
    "!send <file> <remote-file>   - send file to server [PASV/PORT][STOR <file>]  ",
    "system                      - show system [SYST]  ",
    "user                        - login to the server [USER/PASS]",
  ],
};

/* sendAction(
  client,
  {
    action: "Login",
    username: "user",
    password: "pass",
  },
  callback
); */

// TODO: only for test here
logger.log("Welcome to FTPC!");
console.log("Welcome to FTPC!");
// const delay = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
// await delay(3000); /// waiting 1 second.

ftpc.run();

/* 
help:
binary                      - set to binary mode [TYPE I]
bye                         - exit()
cd <dir>                    - cd directory [CWD]
dir                         - list current directory [PASV/PORT][LIST]
exit                        - exit() [QUIT]
get <remote-file> <file>    - get remote-file to file [PASV/PORT][RETR]
help                        - show this help page 
ls                          - list current directory [PASV/PORT][LIST]
login                       - Login to the server   [USER/PASS]
mkdir <dir>                 - make directory  [MKD]
passive                     - switch PASV/PORT mode  
pwd                         - show current directory [PWD]
quit                        - exit() [QUIT]
rename <file> <new_name>    - rename file [RNFR/RNTO]
rmdir <dir>                 - remove directory [RMD]
send <file> <remote-file>   - send file to server [PASV/PORT][STOR <file>] 
system                      - show system [SYST] 
user                        - login to the server [USER/PASS]
*/
