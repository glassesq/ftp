/* FTP Client */

import log4js from "log4js";
import { Buffer } from "buffer";
import net from "net";
import fs from "fs";
import { EventEmitter } from "events";

const logger = log4js.getLogger("ftp-service");
logger.level = "debug";

/* gen a react to outside */
function genReaction(message, wait, kill = false) {
  return {
    message: message.trim(),
    wait: wait,
    kill: kill,
  };
}

function genReactionFromReply(reply, wait, kill = false) {
  return {
    message: reply.genMessage().trim(),
    reply: reply,
    wait: wait,
    kill: reply.code == 421 ? true : kill,
  };
}

const reply_proto = {
  genMessage: function (end_break = false) {
    if (!this.message.endsWith("\r\n")) this.message = this.message + "\r\n";
    let msg = "";
    const sentences = this.message.split("\r\n");
    const len = sentences.length - 1;
    for (let i = 0; i < len - 1; i++) {
      msg = msg + this.code + "-" + sentences[i] + "\r\n";
    }
    msg =
      msg + this.code + " " + sentences[len - 1] + (end_break ? "\r\n" : "");
    return msg;
  },
};

function FTPReply(code, msg) {
  const r = Object.create(reply_proto);
  r.code = code;
  r.message = msg;
  r.kind = r.code >= 100 && r.code <= 199 ? "mark" : "response";
  if (r.code == 421) r.quit = true;
  else r.quit = false;
  return r;
}

const request_proto = {
  sendRequest: function (ftp_socket) {
    return ftp_socket.write(this.command + " " + this.params + "\r\n");
  },
};

function FTPRequest(command, param) {
  const r = Object.create(request_proto);
  r.command = command;
  r.param = param;
  return r;
}

const ftpservice = {
  runFTP: function () {
    logger.info(
      "running on " +
        this.ftp_socket.localAddress +
        ":" +
        this.ftp_socket.localPort
    );
  },
  /* return value show if the data is freshed to kernel buffer */
  runAct: function (action) {
    if ("handle" + action.action in this) {
      logger.debug("Action get " + action.action);
      return this["handle" + action.action](action);
    } else {
      logger.warn("Unknown action " + action.action);
      return { status: "unknown", msg: "unknown action." };
    }
  },
  /* checkLine will try to remove those substring ends in \r\n but not match reply format */
  /* checkLine will remove the \r\n from the endline [for print] */
  checkLine: function (raw) {
    const l = raw.indexOf("\r\n") + 2; /* [0, len) */
    if (l <= 5) return { msg: null, len: l }; /* ill format 123 \r\n */
    const re = /[1-5][0-9][0-9]/i;
    if (raw.search(re) != 0) return { msg: null, len: l };
    if (raw[3] != " " && raw[3] != "-") return { msg: null, len: l };

    return {
      msg: raw.substring(4, l),
      endline: raw[3] == " " ? true : false,
      code: parseInt(raw.substring(0, 3)),
      len: l /* length of the line */,
    };
  },
  /* check Reply from this.buffer */
  checkReply: function () {
    let message = "";
    let length = 0;
    let reply_code = -1;

    while (true) {
      const { msg, endline, code, len } = this.checkLine(
        this.buffer.slice(length)
      );

      if (msg == null) return null; /* reply not complete */
      if (msg == "ill-format") {
        length += len;
        /* checkLine find some ill-format substring */
        /* we will throw it when a complete reply received */
        continue;
      }

      /* msg is not null */
      message = message.concat(msg);
      length += len;

      if (reply_code == -1) {
        reply_code = code;
      } else if (reply_code != -1 && code != reply_code) {
        logger.warn("recieve a ill-format reply" + buffer);
      }

      if (endline) break;
    }

    this.buffer = this.buffer.slice(length);

    /* return reply and new buffer */
    return FTPReply(reply_code, message);
  },
  waitReply: function (data) {
    logger.debug("[waitReply] some data come");
    this.buffer = this.buffer.concat(data);
    logger.debug(JSON.stringify(this.buffer));

    if (this.dactive) {
      /* ftp socket transfer should be transfer when the data socket is active. i.e. waiting data to come. */
      logger.debug("[waitReply] hang on");
      return;
    }

    const reply = this.checkReply();
    if (reply == null) return;

    /* when receive a reply, call it callback */
    const r = this.callbacks.at(0);
    if (!r) logger.warn("[waitReply] callback lost");
    if (reply.kind == "response") this.callbacks.shift();
    r?.(reply);

    if (this.buffer.length > 0) {
      this.waitReply("");
    }
  },
  waitData: function (data) {
    logger.debug("[waitData] some data come" + data.length);
    this.dbuffer = this.dbuffer.concat(data);
    /* big file save. use rv of callback */
    const r = this.databack;
    if (!r) logger.warn("[waitData] callback lost");
    r?.(data);
  },
  handleRmdir: function (action) {
    logger.info("rmdir [" + action.dir + "]");
    this.callbacks.push((reply) => {
      this.react?.(genReactionFromReply(reply, false));
    });
    this.writeRaw("RMD " + action.dir + "\r\n");
  },
  handleMkdir: function (action) {
    logger.info("mkdir [" + action.dir + "]");
    this.callbacks.push((reply) => {
      this.react?.(genReactionFromReply(reply, false));
    });
    this.writeRaw("MKD " + action.dir + "\r\n");
  },
  handleCwd: function (action) {
    logger.info("cwd [" + action.dir + "]");
    this.callbacks.push((reply) => {
      this.react?.(genReactionFromReply(reply, false));
    });
    this.writeRaw("CWD " + action.dir + "\r\n");
  },
  handlePwd: function (action) {
    logger.info("pwd");
    this.callbacks.push((reply) => {
      this.react?.(genReactionFromReply(reply, false));
    });
    this.writeRaw("PWD\r\n");
  },
  handleSystem: function (action) {
    logger.info("system");
    this.callbacks.push((reply) => {
      this.react?.(genReactionFromReply(reply, false));
    });
    this.writeRaw("SYST\r\n");
  },
  handleBinary: function (action) {
    logger.info("binary");
    this.callbacks.push((reply) => {
      this.react?.(genReactionFromReply(reply, false));
    });
    this.writeRaw("TYPE I\r\n");
  },
  handleUser: function (action) {
    logger.info("user [" + action.username + "] login ");
    this.callbacks.push((reply) => {
      this.react?.(genReactionFromReply(reply, false));
    });
    this.writeRaw("USER " + action.username + "\r\n");
  },
  handlePass: function (action) {
    logger.info("password is [" + action.password + "]");
    this.callbacks.push((reply) => {
      this.react?.(genReactionFromReply(reply, false));
    });
    this.writeRaw("PASS " + action.password + "\r\n");
  },
  handleQuit: function (action) {
    logger.info("quit");
    this.callbacks.push((reply) => {
      this.react?.(genReactionFromReply(reply, false, true));
    });
    this.writeRaw("QUIT\r\n");
  },
  handleRename: function (action, step = "rnfr") {
    if (step == "rnfr") {
      this.callbacks.push((reply) => {
        if (reply.code == 350) {
          this.react?.(genReactionFromReply(reply, true));
          this.handleRename(action, "rnto");
        } else {
          this.react?.(genReactionFromReply(reply, false));
        }
      });
      this.writeRaw("RNFR " + action.from + "\r\n");
    } else if (step == "rnto") {
      logger.debug("[handleRename] rnto");
      this.callbacks.push((reply) => {
        if (parseInt(reply.code / 100) == 2) {
          this.react?.(genReactionFromReply(reply, false));
        } else {
          this.react?.(genReactionFromReply(reply, false));
        }
      });
      this.writeRaw("RNTO " + action.to + "\r\n");
      // transfer data in data_socket
    }
  },
  handleList: function (action, step = "mode") {
    if (step == "mode") {
      this.prepareMode((reply) => {
        this.handleList(action, "list");
      });
    } else if (step == "list") {
      logger.debug("[handleList] list");
      this.callbacks.push((reply) => {
        if (parseInt(reply.code / 100) == 1) {
          this.react?.(genReactionFromReply(reply, true));
          this.dactive = true;
          logger.debug("dactive: true");
        } else if (reply.code == 226) {
          this.react?.(genReactionFromReply(reply, true));
          this.handleList(action, "over");
        } else {
          this.react?.(genReactionFromReply(reply, false));
        }
      });
      this.databack = (data) => {};
      this.writeRaw("LIST\r\n");
      // transfer data in data_socket
    } else if (step == "over") {
      // over list
      const msg = this.dbuffer;
      this.dbuffer = "";
      this.react?.(genReaction(msg, false));
      logger.log("[handleList] over");
    }
  },
  writeRaw: function (raw) {
    logger.debug("write " + raw.trim() + " to ftp_socket");
    const ret = this.ftp_socket.write(raw);
    return ret;
  },
  handleTest: function (action) {
    return {
      status: this.writeRaw(action.msg),
      msg: "test message.",
    };
  },
  handlePassive: function (action) {
    if (this.mode == "pasv") this.mode = "port";
    else this.mode = "pasv";
    this.react?.(
      genReaction("Current mode is " + this.mode.toUpperCase(), false)
    );
  },
  handleError: function () {
    logger.warn("some error happened");
  },
  /* prepare pasv / port mode for data transfer */
  prepareMode: function (callback = null) {
    /* checkmode */
    this.d_socket?.end();
    this.dserver_socket?.close();
    if (this.mode == "pasv") {
      this.callbacks.push((reply) => {
        if (parseInt(reply.code / 100) != 2) {
          this.react?.(genReactionFromReply(reply, false));
          return;
        }
        logger.debug("[mode prepare]" + reply.genMessage());
        const regex = /((([0-9]{1,3}),){5})([0-9]{1,3})/g;
        const info = reply.message.match(regex)?.[0];
        if (info != null) {
          const ipre = /((([0-9]{1,3}),){3})([0-9]{1,3})/g;
          this.d_pasvhost = info.match(ipre)?.[0].replaceAll(",", ".");
          if( this.d_pasvhost.startsWith("0.") || this.d_pasvhost.startsWith("127.") || this.d_pasvhost.startWith("172.") ) {
            this.pasvhost = this.server_ip;
          }
          const num = info.split(",");
          this.d_pasvport = parseInt(num.at(-2)) * 256 + parseInt(num.at(-1));

          logger.info(
            "[prepareMode] PASV: " + this.d_pasvhost + ":" + this.d_pasvport
          );

          this.d_socket = net.createConnection(
            this.d_pasvport,
            this.d_pasvhost,
            () => {
              logger.debug("d_socket connection created.");
              callback?.(reply);
              this.waitData("");
            }
          );
          this.d_socket.on("data", (data) => this.waitData.bind(this)(data));
          this.d_socket.on("close", () => {
            logger.debug("pasv d_socket off");
            this.dactive = false;
            this.waitReply("");
          });
        }
        // get dserver_host/port from reply.
        logger.debug("[prepareMode] try to call the callback");
      });
      this.writeRaw("PASV\r\n");
    } else if (this.mode == "port") {
      // prepare listen socket for port mode
      this.dserver_socket = net.createServer((sock) => {
        // TODO: check permission
        logger.debug("sock got");
        this.d_socket = sock;
        this.d_socket.on("data", (data) => this.waitData.bind(this)(data));
        this.d_socket.on("close", () => {
          logger.debug("port d_socket off");
          this.dactive = false;
          this.waitReply("");
        });
        this.waitData("");
        // this.dserver_socket.close();
      });
      this.dserver_socket.maxConnections = 1;
      this.dserver_socket = this.dserver_socket.listen();
      this.dserver_socket.on("listening", () => {
        const port = this.dserver_socket.address().port;
        let msg =
          this.ftp_socket.localAddress +
          "." +
          parseInt(port / 256) +
          "." +
          (port % 256);
        msg = msg.replaceAll(".", ",");
        logger.debug("prepare to send " + msg);
        this.callbacks.push((reply) => {
          logger.debug("callbacks:" + reply.code + " " + reply.message);
          if (parseInt(reply.code / 100) != 2) {
            this.react?.(genReactionFromReply(reply, false));
            return;
          }
          logger.debug("callbacks:" + reply.code + " " + reply.message);
          callback?.(reply);
        });
        this.writeRaw("PORT " + msg + "\r\n");
      });
      // logger.debug("listen to " + this.dserver_socket.port);

      // get dserver_host/port from reply.
      // logger.debug("[prepareMode] try to call the callback");
      // this.callbacks.push((reply) => callback?.(reply));
      // this.writeRaw("PORT\r\n");
    } else return false;
    return false;
  },
  handleSend: function (action, step = "mode") {
    if (step == "mode") {
      this.prepareMode((reply) => {
        this.handleSend(action, "stor");
      });
    } else if (step == "stor") {
      logger.debug("[handleStor] stor");
      if (!fs.existsSync(action.local)) {
        this.react?.(
          genReaction("local file " + action.local + "not exists.", false)
        );
        return;
      }
      fs.open(action.local, "r", (err, fd) => {
        if (err) {
          logger.debug("fs fail to open." + action.local);
          this.react?.(genReaction("File system not allowed", false));
          return;
        }
        this.fd = fd;
        this.callbacks.push((reply) => {
          if (parseInt(reply.code / 100) == 1) {
            this.react?.(genReactionFromReply(reply, true));
            this.dactive = true;
            logger.debug("dactive: true");
          } else if (reply.code == 226) {
            this.handleSend(action, "over");
            this.react?.(genReactionFromReply(reply, false));
          } else {
            this.react?.(genReactionFromReply(reply, false));
          }
        });
        this.databack = (data) => {
          logger.debug(data);
          while (true) {
            const buffer = Buffer.alloc(8192);
            let n = fs.readSync(this.fd, buffer, 0, 8192);
            if (n <= 0) {
              break;
            } else {
              this.d_socket.write(buffer.subarray(0, n));
              if (n != 8192) break;
            }
          }
          this.d_socket.end();
        };
        this.writeRaw("STOR " + action.remote + "\r\n");
      });
      // transfer data in data_socket
    } else if (step == "over") {
      logger.log("[handleStor] over");
      fs.close(this.fd);
      this.dbuffer = "";
    }
  },

  handleGet: function (action, step = "mode") {
    if (step == "mode") {
      this.prepareMode((reply) => {
        this.handleGet(action, "retr");
      });
    } else if (step == "retr") {
      logger.debug("[handleRetr] retr");
      if (fs.existsSync(action.local)) {
        this.react?.(
          genReaction("local file " + action.local + " exists.", false, false)
        );
        return;
      }
      fs.open(action.local, "w", (err, fd) => {
        if (err) {
          logger.debug("fs fail to open." + action.local);
          this.react?.(genReaction("File system not allowed", false));
          return;
        }
        this.fd = fd;
        this.callbacks.push((reply) => {
          if (parseInt(reply.code / 100) == 1) {
            this.react?.(genReactionFromReply(reply, true));
            this.dactive = true;
            logger.debug("dactive: true");
          } else if (reply.code == 226) {
            this.d_socket.end();
            this.handleGet(action, "over");
            this.react?.(genReactionFromReply(reply, false));
          } else {
            this.react?.(genReactionFromReply(reply, false));
          }
        });
        this.databack = (data) => {
          if (data == "") return;
          fs.write(this.fd, data, () => {});
          logger.debug(data);
        };
        this.writeRaw("RETR " + action.remote + "\r\n");
      });
      // transfer data in data_socket
    } else if (step == "over") {
      logger.log("[handleGet] over");
      fs.close(this.fd);
      this.dbuffer = "";
    }
  },
  handleClose: function () {
    logger.info("The FTP service is fully closed");
  },
  killService: function () {
    logger.info("[killService] someone try to kill service");
    this.ftp_socket?.end();
    this.d_socket?.end();
    this.dserver_socket?.close();
  },
  greet: function (reply) {},
  startService: function () {
    if (this.server_ip != null) {
      logger.debug("ip check ok");
      this.ftp_socket = net.createConnection(
        this.server_port,
        this.server_ip,
        this.runFTP.bind(this)
      );
      this.callbacks.push((reply) => {
        this.react(genReactionFromReply(reply, false));
      }); /* greeting message */
      this.ftp_socket.on("error", this.handleError.bind(this));
      this.ftp_socket.on("data", (data) => this.waitReply.bind(this)(data));
      this.ftp_socket.on("close", this.handleClose.bind(this));
      return true;
    }
    return false;
  },
  setAddress: function (host, port) {
    this.server_ip = host;
    this.server_port = port;
  },
  setReact: function (react) {
    this.react = react;
  },
};

function ftpclient() {
  this.server_ip = "0.0.0.0";
  this.server_port = 5001;
  this.ftp_socket = null; /* ftp socket for commands */
  this.d_socket = null; /* data socket for data transfering */
  this.dserver_socket = null; /* data server socket for PORT mode */
  this.dserver_host = null;
  this.dserver_port = null;
  this.emitter = new EventEmitter();
  this.buffer = ""; /* buffer for reading */
  this.dbuffer = ""; /* dbuffer */
  this.mode = "port"; /* pasv or port */
  this.react = null; /* the react of client outside */
  this.callbacks = []; /* queue when wait for a response*/
  this.databack = null;
  this.dactive = false;
  this.fd = null;
}

function initFTP() {
  Object.assign(ftpclient.prototype, ftpservice);
  const client = new ftpclient();
  return client;
}

export { initFTP };
