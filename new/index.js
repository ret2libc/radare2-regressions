const promiseConcurrency = 8;
const timeoutFuzzed = 60 * 1000;

const co = require('co');
const colors = require('colors/safe');
const promisify = require('util').promisify;
const walk = require('walk').walk;
const fs = require('fs');
const fsWriteFile = promisify(fs.writeFile);
const tmp = require('tmp');
const zlib = require('zlib');
const path = require('path');
const spawn = require('child_process').spawn;
const spawnSync = require('child_process').spawnSync;
const r2promise = require('r2pipe-promise');
const common = require('./common');
const promiseLimit = require('promise-limit');

const limit = promiseLimit(promiseConcurrency);

if (process.env.TRAVIS || process.env.APPVEYOR) {
  process.env.NOOK = 1;
}

function newPromise (cb) {
  return limit(_ => new Promise(cb));
}

function canRun (test) {
  if (!test.needs_plugins) {
    return true;
  }

  let allPresent = true;
  for (let i = 0; i < test.needs_plugins.length; i++) {
    let p = test.needs_plugins[i];
    if (p.startsWith('asm.')) {
      p = p.slice(4)
      args = ['-c', 'e asm.arch=??', '-qcq', '-']
    } else if (p.startsWith('anal.')) {
      p = p.slice(5)
      args = ['-c', 'e anal.arch=??', '-qcq', '-']
    } else if (p.startsWith('bin.')) {
      p = p.slice(4)
      args = ['-c', 'iL', '-qcq', '-']
    } else if (p.startsWith('lang.')) {
      p = p.slice(5)
      args = ['-c', 'Ll', '-qcq', '-']
    } else if (p.startsWith('core.')) {
      p = p.slice(5)
      args = ['-c', 'Lc', '-qcq', '-']
    } else if (p.startsWith('hash.')) {
      p = p.slice(5)
      args = ['-c', 'Lh', '-qcq', '-']
    } else if (p.startsWith('io.')) {
      p = p.slice(3)
      args = ['-c', 'Lo', '-qcq', '-']
    } else {
      console.log('Wrong plugin: ' + p)
      process.exit(1)
    }
    const supported = spawnSync(r2bin, args).output.toString()
    allPresent = allPresent & (supported.indexOf(p) != -1);
  }
  return allPresent;
}

// support node < 8
if (!String.prototype.padStart) {
  // XXX
  String.prototype.padStart = function padStart (targetLength, padString) {
    targetLength = targetLength >> 0; // floor if number or convert non-number to 0;
    padString = String(padString || ' ');
    if (this.length > targetLength) {
      return String(this);
    }
    targetLength = targetLength - this.length;
    if (targetLength > padString.length) {
      padString += padString.repeat(targetLength / padString.length); // append to original to ensure we are longer than needed
    }
    return padString.slice(0, targetLength) + String(this);
  };
}

// set this to false to avoid creating files
let useScript = true;

/* radare2 binary name */
const r2bin = 'radare2';

class NewRegressions {
  constructor (argv, cb) {
    this.argv = argv;
    this.queue = [];
    this.report = {
      total: 0,
      success: 0,
      failed: 0,
      broken: 0,
      fixed: 0,
      totaltime: 0
    };
    useScript = !this.argv.c;
    this.verbose = this.argv.verbose || this.argv.v;
    this.interactive = this.argv.interactive || this.argv.i;
    this.debase64 = this.argv.debase64;
    this.format = this.argv.format;
    if ((this.debase64 || this.format) && process.platform === 'win32') {
      // since r2r on Windows modifies tests on-the-fly...
      console.log('Do not run --debase64 or --format on Windows!');
      process.exit(1);
    }
    this.promises = [];
    r2promise.open('-').then(r2 => {
      this.r2 = r2;
      cb(null, r2);
    }).catch(e => {
      cb(e);
    });
    this.start = new Date();
  }
  callbackFromPath (from) {
    for (let row of [
      [path.join('db', 'anal'), this.runTest],
      [path.join('db', 'archos'), this.runTest],
      [path.join('db', 'cmd'), this.runTest],
      [path.join('db', 'esil'), this.runTest],
      [path.join('db', 'extras'), this.runTest],
      [path.join('db', 'formats'), this.runTest],
      [path.join('db', 'io'), this.runTest],
      [path.join('db', 'tools'), this.runTest]
    ]) {
      const [txt, cb] = row;
      if (from.indexOf(txt) !== -1) {
        return cb;
      }
    }
    return null;
  }

  quit () {
    const promise = this.r2 !== null
      ? this.r2.quit()
      : new Promise(resolve => resolve());
    this.r2 = null;
    return promise;
  }

  runTestAsm (test, cb) {
    const self = this;
    return newPromise((resolve, reject) => {
      try {
        co(function * () {
          try {
            if (test.args) {
              self.r2.cmd(test.args);
            }
            test.stdout = yield self.r2.cmd(test.cmd);
            return resolve(cb(test));
          } catch (e) {
            return reject(e);
          }
        });
      } catch (e) {
        console.error(e);
        reject(e);
      }
    });
  }

  runTestJson (test, cb) {
    const self = this;
    return newPromise((resolve, reject) => {
      try {
        co(function * () {
          try {
            if (test.path) {
              self.r2.cmd('o ' + test.path, '; o-!; aaa');
            }
            test.stdout = yield self.r2.cmd(test.cmd);
            return resolve(cb(test));
          } catch (e) {
            return reject(e);
          }
        });
      } catch (e) {
        console.error(e);
        reject(e);
      }
    });
  }

  runTestFuzz (test, cb) {
    return newPromise((resolve, reject) => {
      try {
        co(function * () {
          const args = ['-c', '?e init', '-qcq', '-A', test.path];
          test.birth = new Date();
          const child = spawnSync(r2bin, args, {timeout: timeoutFuzzed});
          test.death = new Date();
          test.lifetime = test.death - test.birth;
          if (child.error) {
            test.fuzz = true;
            test.expectErr = 'N';
            test.stderr = 'X';
            test.spawnArgs = args;
            test.cmdScript = '';
            return reject(cb(test));
          } else {
            return resolve(cb(test));
          }
        });
      } catch (e) {
        reject(e);
      }
    });
  }

  runTest (test, cb) {
    return newPromise((resolve, reject) => {
      if (this.argv.l) {
        console.log(test.from.replace('db/', ''), test.name);
        return resolve();
      }
      co(function * () {
        const args = [
          '-escr.utf8=0',
          '-escr.color=0',
          '-escr.interactive=0',
          '-N',
          '-Q'
        ];
        if (process.env.APPVEYOR && process.env.ANSICON === undefined) {
          process.env['ANSICON'] = 'True';
        }
        // Append custom r2 args
        if (test.args && test.args.length > 0) {
          args.push(...test.args.split(' '));
        }
        try {
          if (useScript) {
            // TODO much slower than just using -c
            test.tmpScript = yield createTemporaryFile();
            // TODO use yield here
            yield fsWriteFile(test.tmpScript, test.cmdScript);
            args.push('-i', test.tmpScript);
          } else {
            if (!test.cmds && test.cmdScript) {
              test.cmds = test.cmdScript.split('\n');
            }
            args.push('-c', test.cmds.join(';'));
          }
          if (!test.file) {
            test.file='-';
          }
          // Append testfile
          args.push(...test.file.split(' '));

          let res = '';
          let ree = '';
          test.spawnArgs = args;

          // Set or unset NOPLUGINS to speedup launch time
          if (test.from.indexOf('extras') !== -1) {
            delete process.env.RABIN2_NOPLUGINS;
            delete process.env.RASM2_NOPLUGINS;
            delete process.env.R2_NOPLUGINS;
          } else {
            process.env.RABIN2_NOPLUGINS = 1;
            process.env.RASM2_NOPLUGINS = 1;
            process.env.R2_NOPLUGINS = 1;
          }

          if (!canRun(test)) {
            console.log('Skipping ' + test.name + ' because some plugins are missing')
            test.broken = true;
            resolve(cb(test));
	  }

          const child = spawn(r2bin, args);
          test.birth = new Date();
          child.stdout.on('data', data => {
            res += data.toString();
          });
          child.stderr.on('data', data => {
            ree += data.toString();
          });
          child.on('close', data => {
            test.death = new Date();
            try {
              if (test.tmpScript) {
                // TODO use yield
                fs.unlinkSync(test.tmpScript);
                test.tmpScript = null;
              }
            } catch (e) {
              console.error(e);
              // ignore
            }
            test.lifetime = test.death - test.birth;
            test.stdout = res;
            test.stderr = ree;
            resolve(cb(test));
          });
        } catch (e) {
          console.error(e);
          reject(e);
        }
      });
    });
  }

  runTests (source, lines) {
    let test = {from: source};
    const editMode = {
      match: false,
      name: '',
      enabled: false,
      str: ''
    };
    // edit is work in progress. aka not working at all
    if (this.argv.e) {
      editMode.match = true;
      editMode.name = 'cmd_graph';
      process.exit(1);
    }
    const delims = /['"%]/;
    for (let i = 0; i < lines.length; i++) {
      let l = lines[i];
      const line = l.trim();

      if (line.length === 0 || line[0] === '#') {
        continue;
      }

      if (editMode.enabled) {
        if (editMode.match) {
          console.log(line);
        }
        if (line === 'RUN') {
          editMode.match = false;
        }
        continue;
      }

      // Execute json tests
      if (source.indexOf('json' + path.sep) !== -1) {
        let tests = parseTestJson(source, line);
        for (let t of tests) {
          this.promises.push(this.runTestJson.bind(this)(t, this.checkTestResult.bind(this)));
        }
        continue;
      }

      // Execute asm tests
      if (source.indexOf('asm') !== -1 && source.indexOf('rasm2') === -1) {
        let tests = parseTestAsm(source, line);
        for (let t of tests) {
          this.promises.push(this.runTestAsm.bind(this)(t, this.checkTestResult.bind(this)));
        }
        continue;
      }

      // Execute normal test
      if (line === 'RUN') {
        const testCallback = this.callbackFromPath(test.from);
        if (testCallback !== null) {
          this.promises.push(testCallback.bind(this)(test, this.checkTestResult.bind(this)));
          test = {from: source};
          continue;
        }
      }

      const eq = l.indexOf('=');

      if (eq === -1) {
        console.error('Action' + l + ' seems invalid (' + source + ')');
        throw new Error('Invalid action: ' + l);
      }

      const k = l.substring(0, eq);
      const v = l.substring(eq + 1);
      const vt = v.trim();
      switch (k) {
        case 'NAME':
          test.name = v;
          if (editMode.enabled && editMode.name === v) {
            editMode.match = true;
          }
          break;
        case 'PATH':
          test.path = v;
          break;
        case 'ARGS':
          test.args = v || [];
          break;
	case 'NEEDS_PLUGINS':
	  test.needs_plugins = v ? v.split(' '): null;
	  break;
        case 'CMDS':
          if (vt.startsWith('<<')) {
            const endString = vt.substring(2);
            test.cmdScript = '';
            i++;
            while (!lines[i].startsWith(endString)) {
              test.cmdScript += lines[i] + '\n';
              i++;
            }
            if (endString !== 'EOF') {
              i--;
            }
          } else {
            const delim = vt.charAt(0);
            if (delims.test(delim)) {
              const startDelim = v.indexOf(delim);
              let endDelim = v.indexOf(delim, startDelim + 1);
              if (endDelim === -1) {
                test.cmdScript = v.substring(startDelim + 1) + '\n';
                i++;
                while ((endDelim = lines[i].indexOf(delim)) === -1) {
                  test.cmdScript += lines[i] + '\n';
                  i++;
                }
                test.cmdScript += lines[i].substring(0, endDelim);
              } else {
                test.cmdScript = v.substring(startDelim + 1, endDelim) + '\n';
              }
            } else {
              test.cmdScript = v ? v + '\n' : v;
            }
          }
          test.cmds = test.cmdScript ? test.cmdScript.trim().split('\n') : [];
          break;
/*
        case 'CMDS64':
          test.cmdScript = debase64(v);
          test.cmds = test.cmdScript ? test.cmdScript.trim().split('\n') : [];
          break;
*/
        case 'ARCH':
          test.arch = v;
          break;
        case 'BITS':
          test.bits = v;
          break;
        case 'BROKEN':
          test.broken = true;
          break;
        case 'EXPECT':
          test.expect64 = false;
          if (vt.startsWith('<<')) {
            const endString = vt.substring(2);
            test.expectEndString = endString;
            test.expect = '';
            i++;
            while (lines[i] !== undefined && !lines[i].startsWith(endString)) {
              test.expect += lines[i] + '\n';
              i++;
            }
            if (lines[i] === undefined) {
              throw new Error('Unexpected EOF in EXPECT -- did you forget a ' + endString + '?');
            }
            if (endString !== 'EOF') {
              i--;
            }
          } else {
            const delim = vt.charAt(0);
            if (delims.test(delim)) {
              test.expectDelim = delim;
              const startDelim = v.indexOf(delim);
              let endDelim = v.indexOf(delim, startDelim + 1);
              if (endDelim === -1) {
                test.expect = v.substring(startDelim + 1) + '\n';
                i++;
                while ((endDelim = lines[i].indexOf(delim)) === -1) {
                  test.expect += lines[i] + '\n';
                  i++;
                }
                test.expect += lines[i].substring(0, endDelim);
              } else {
                test.expect = v.substring(startDelim + 1, endDelim); // No newline added
              }
            } else {
              test.expect = v + '\n';
            }
          }
          break;
/*
        case 'EXPECT64':
          test.expect = debase64(v);
          test.expect64 = true;
          break;
*/
        case 'EXPECT_ERR':
          if (vt.startsWith('<<')) {
            const endString = vt.substring(2);
            test.expectErrEndString = endString;
            test.expectErr = '';
            i++;
            while (lines[i] !== undefined && !lines[i].startsWith(endString)) {
              test.expectErr += lines[i] + '\n';
              i++;
            }
            if (lines[i] === undefined) {
              throw new Error('Unexpected EOF in EXPECT_ERR -- did you forget a ' + endString + '?');
            }
            if (endString !== 'EOF') {
              i--;
            }
          } else {
            const delim = vt.charAt(0);
            if (delims.test(delim)) {
              test.expectErrDelim = delim;
              const startDelim = v.indexOf(delim);
              let endDelim = v.indexOf(delim, startDelim + 1);
              if (endDelim === -1) {
                test.expectErr = v.substring(startDelim + 1) + '\n';
                i++;
                while ((endDelim = lines[i].indexOf(delim)) === -1) {
                  test.expectErr += lines[i] + '\n';
                  i++;
                }
                test.expectErr += lines[i].substring(0, endDelim);
              } else {
                test.expectErr = v.substring(startDelim + 1, endDelim); // No newline added
              }
            } else {
              test.expectErr = v + (v.length === 0 ? '' : '\n');
            }
          }
          break;
/*
        case 'EXPECT_ERR64':
          test.expect = debase64(v);
          break;
*/
        case 'FILE':
          test.file = v;
          break;
        default:
          throw new Error('Invalid database, key = (' + k + ')');
      }
    }
    function complete (x) {
      //
    }
    if (Object.keys(test) !== 0) {
      if (test.file && test.cmds) {
        this.promises.push(this.runTest(test, complete));
      }
    }
  }

  runFuzz (dir, files) {
    let test = {};
    for (let f of files) {
      test = {from: dir, name: 'fuzz', path: path.join(dir, f)};
      this.promises.push(this.runTestFuzz.bind(this)(test, this.checkTestResult.bind(this)));
    }
  }

  load (fileName, cb) {
    this.name = fileName;
    const pathName = path.join(__dirname, fileName);
    const blob = fs.readFileSync(pathName);
    // do we really need to support gzipped tests?
    zlib.gunzip(blob, (err, data) => {
      let tests;
      if (err) {
        tests = blob.toString();
      } else {
        tests = data.toString();
      }
      if (process.platform === 'win32') {
        tests = tests.replace(/\/dev\/null/g, 'nul').replace(/\r\n/g, '\n').split('\n');
        for (let i = 0; i < tests.length; i++) {
          if (tests[i].startsWith('!') || tests[i].startsWith('CMDS=!')) {
            tests[i] = tests[i].replace(/\${(\S+?)}/g, '%$1%')
              .replace(/awk "{print \\\$1}"/g, "sed 's/^[ \\t]*//;s/[ \\t]*$//'");
          }
        }
      } else {
        tests = tests.split('\n');
      }
      if (this.argv.grep !== undefined) {
        return cb(null, {});
      }
      if (this.argv.debase64) {
        let newTests = [];
        let writeTests = false;
        process.stdout.write('Checking for base64 in ' + fileName + '...');
        for (let i = 0; i < tests.length; i++) {
          let line = tests[i].trim();
          if (line.startsWith('CMDS64=')) {
            writeTests = true;
            line = debase64(line.substring(7)).trimStart().replace(/\n+$/, '');
            newTests.push('CMDS=<<EOF');
            newTests.push(line);
            newTests.push('EOF');
          } else if (line.startsWith('EXPECT64=')) {
            writeTests = true;
            line = debase64(line.substring(9)).replace(/\n+$/, '');
            if (line.startsWith('[') && line.endsWith(']') ||
                line.startsWith('{') && line.endsWith('}')) { // JSON
              let delim = common.getSuitableDelim(line);
              newTests.push('EXPECT=' + delim + line + '\n' + delim);
            } else {
              newTests.push('EXPECT=<<EOF');
              if (line !== '') {
                newTests.push(line);
              }
              newTests.push('EOF');
            }
          } else if (line.startsWith('CMDS=<<EXPECT64')) {
            writeTests = true;
            newTests.push('CMDS=<<EXPECT');
          } else {
            newTests.push(tests[i]);
          }
        }
        if (writeTests) {
          fs.writeFileSync(pathName, newTests.join('\n'));
          console.log('DEBASE64ED');
        } else {
          console.log('OK');
        }
        if (this.argv.format) {
          tests = newTests;
          // fallthrough
        } else {
          return cb(null, {});
        }
      }
      if (this.argv.format) {
        let newTests = [];
        let writeTests = false;
        let prevLineRUN = false;
        process.stdout.write('Checking format of ' + fileName + '...');
        for (let i = 0; i < tests.length; i++) {
          if (prevLineRUN) {
            prevLineRUN = false;
            if (tests[i].trim() !== '') {
              writeTests = true;
              newTests.push('');
            }
          }
          newTests.push(tests[i]);
          if (tests[i].trim() === 'RUN') {
            prevLineRUN = true;
          }
        }
        if (writeTests) {
          fs.writeFileSync(pathName, newTests.join('\n'));
          console.log('FIXED');
        } else {
          console.log('OK');
        }
        return cb(null, {});
      }
      this.runTests(fileName, tests);
      Promise.all(this.promises).then(res => {
        this.printReport();
        cb(null, res);
      }).catch(err => {
        console.log(err);
        cb(err);
      });
    });
  }

  loadFuzz (dir, cb) {
    console.log('[--]', 'fuzz binaries');
    const fuzzed = fs.readdirSync(dir);
    this.runFuzz(dir, fuzzed);
    Promise.all(this.promises).then(res => {
      this.printReport();
      cb(null, res);
    }).catch(err => {
      console.log(err);
      cb(err);
    });
  }

  checkTest (test, cb) {
    if (process.platform === 'win32') {
      /* Delete \r on windows.
       * Note that process.platform is always win32 even on Windows 64 bits */
      if (typeof test.stdout !== 'undefined') { // && test.expect) {
        test.stdout = test.stdout.replace(/\r/g, '');
      }
      if (typeof test.stderr !== 'undefined') {
        test.stderr = test.stderr.replace(/\r/g, '');
      }
    }

    /* Check test output, if it's the same, the test passes */
    if (test.check === undefined) {
      if (test.expect !== undefined) {
        test.stdoutFail = (test.expect64 || test.expect64 === undefined)
          ? test.expect.trim() !== test.stdout.trim()
          : test.expect !== test.stdout;
      } else {
        test.stdoutFail = false;
      }
      test.stderrFail = test.expectErr !== undefined ? test.expectErr !== test.stderr : false;
      test.passes = !test.stdoutFail && !test.stderrFail;
    } else {
      test.check(test);
    }

    const status = (test.passes)
      ? (test.broken ? colors.yellow('[FX]') : colors.green('[OK]'))
      : (test.broken ? colors.blue('[BR]') : colors.red('[XX]'));
    this.report.total++;
    if (test.passes) {
      if (test.broken) {
        this.report.fixed++;
      } else {
        this.report.success++;
      }
    } else {
      if (test.broken) {
        this.report.broken++;
      } else {
        this.report.failed++;
      }
    }

    /* Hack to hide undefined */
    if (test.path === undefined) {
      test.path = '';
    }
    if (test.lifetime === undefined) {
      test.lifetime = '';
    }
    if ((process.env.NOOK && status !== colors.green('[OK]')) || !process.env.NOOK) {
      process.stdout.write('\x1b[0K\r' + status + ' ' + test.from + ' ' + colors.yellow(test.name) + ' ' + test.path + ' ' + test.lifetime + (this.verbose ? '\n' : '\r'));
    }
    return test.passes;
  }

  checkTestResult (test, cb) {
    const testHasFailed = !this.checkTest(test);

    if (this.interactive) {
      this.verbose = true;
    }
    if (!this.verbose && (test.broken || test.fixed)) {
      return;
    }
    /* Do not show diff if TRAVIS or APPVEYOR and if test is broken */
    if ((process.env.TRAVIS || process.env.APPVEYOR) && test.broken) {
      return;
    }
    if (testHasFailed) {
      console.log('\n$ r2', test.spawnArgs ? test.spawnArgs.join(' ') : '');
      if (test.cmdScript !== undefined) {
        console.log(test.cmdScript);
      }

      let showHeaders = test.stderrFail;
      if (test.stdoutFail) {
        if (showHeaders) {
          console.log('--> stdout\n');
        }
        common.showDiff(test.expect, test.stdout);
      }
      if (test.stdoutFail && test.stderrFail) {
        console.log();
      }
      if (test.stderrFail && test.fuzz === undefined) {
        if (showHeaders) {
          console.log('--> stderr\n');
        }
        // DEBUG console.log("((((", test.expectErr, ")))(((", test.stderr, ")))");
        common.showDiff(test.expectErr, test.stderr);
      }
      /*
      console.log('===');
      if (test.expect !== null) {
        ///console.log('---');
        console.log(colors.magenta(test.expect.trim()));
      }
      if (test.stdout !== null) {
        // console.log('+++');
        console.log(colors.green(test.stdout.trim()));
      }
*/
      // console.log('===');
      if (test.stdoutFail) {
        if (test.expect64) {
          console.log('EXPECT64=' + base64(test.stdout));
        } else if (test.expect64 !== undefined) {
          if (test.expectEndString !== undefined && test.stdout.endsWith('\n')) {
            common.highlightTrailingWs(null, '\nEXPECT=<<' + test.expectEndString + '\n' + test.stdout);
          } else {
            test.expectDelim = common.getSuitableDelim(test.stdout);
            common.highlightTrailingWs(null, '\nEXPECT=' + test.expectDelim + test.stdout + test.expectDelim + '\n');
          }
        }
      }
      if (test.fuzz === undefined) {
        if (!test.stdoutFail && test.stderrFail) {
          console.log();
        }
        if (test.stderrFail) {
          if ((test.stderr.match(/\n/g) || []).length > 1) {
            if (test.expectErrEndString !== undefined && test.stderr.endsWith('\n')) {
              common.highlightTrailingWs(null, 'EXPECT_ERR=<<' + test.expectErrEndString + '\n' + test.stderr);
            } else {
              test.expectErrDelim = common.getSuitableDelim(test.stderr);
              common.highlightTrailingWs(null, 'EXPECT_ERR=' + test.expectErrDelim + test.stderr +
                                         test.expectErrDelim + '\n');
            }
          } else {
            common.highlightTrailingWs(null, 'EXPECT_ERR=' + test.stderr);
          }
        }
      }
      if (this.interactive) {
        //        console.log('TODO: interactive thing should happen here');
      }
      this.queue.push(test);
    }
  }

  printReport () {
    this.report.totaltime = new Date() - this.start;
    const r = {
      name: this.name,
      OK: this.report.success,
      BR: this.report.broken,
      XX: this.report.failed,
      FX: this.report.fixed,
      time: this.report.totaltime
    };
    function n (x) {
      return x.toString().padStart(4);
    }
    const name = (typeof this.name === 'string') ? this.name.padStart(30) : '';

    if ((process.env.NOOK && (r.XX || r.FX)) || !process.env.NOOK) {
      console.log('[**]', name + '  ', 'OK', n(r.OK), 'BR', n(r.BR), 'XX', n(r.XX), 'FX', n(r.FX));
    }
  }

  fixTest (name, expect, cb) {
  }

  editTest (name, expect, cb) {
  }
}

function createTemporaryFile () {
  return new Promise((resolve, reject) => {
    try {
      tmp.file(function (err, filePath, fd, cleanupCallback) {
        if (err) {
          return reject(err);
        }
        resolve(filePath);
      });
    } catch (e) {
      reject(e);
    }
  });
}

function parseTestJson (source, line) {
  const bins = ['../bins/elf/crackme0x00b', '../bins/pe/version_std.exe', '../bins/elf/bomb'];
  let t = {from: source, broken: false};
  let tmp = line.split(' ');

  t.name = tmp[0];
  if (tmp[tmp.length - 1] === 'BROKEN') {
    tmp = tmp.slice(0, tmp.length - 1);
    t.cmd = tmp.join(' ');
    t.broken = true;
  } else {
    t.cmd = line;
    t.broken = false;
  }
  t.check = function(test) {
    try {
      if (test.stdout === '') {
        test.passes = true;
      } else {
        JSON.parse(test.stdout);
        test.passes = true;
      }
    } catch (err) {
      test.passes = false;
      if (t.broken) {
        console.error(colors.blue('[BR] ') + t.cmd);
        console.error(err);
      } else {
        console.error(colors.red.bold('[XX] ') + t.cmd);
        console.error(err);
      }
      
      
    }
  }

  let tests = [];

  for (b of bins) {
    let newtest = Object.assign({'path': b}, t);
    tests.push(newtest);
  }

  return tests;
}

function parseTestAsm (source, line) {
  /* Parse first argument */
  let r2args = [];
  let args = line.match(/(".*?"|[^"\s]+)+(?=\s*|\s*$)/g);
  if (args.length < 3) {
    console.error(colors.red.bold('[XX]', 'Wrong asm test format in ' + source + ':' + line));
    return [];
  }
  let filetree = source.split(path.sep);
  const filename = filetree[filetree.length - 1].split('_');
  if (filename.length > 3) {
    console.error(colors.red.bold('[XX]', 'Wrong asm filename: ' + source));
    return [];
  } else if (filename.length === 2) {
    r2args.push('e asm.bits=' + filename[1]);
  } else if (filename.length === 3) {
    r2args.push('e asm.cpu=' + filename[1]);
    r2args.push('e asm.bits=' + filename[2]);
  }
  r2args.unshift('e asm.arch=' + filename[0]);

  let type = args[0];
  let asm = args[1].split('"').join('');
  let expect = args[2];
  if (args.length >= 4) {
    r2args.push('s ' + args[3]);
  } else {
    r2args.push('s 0');
  }

  /* Generate tests */
  let tests = [];
  for (let c of type) {
    let t = {from: source, broken: false, args: r2args.join(';')};
    t.endianess = false;
    if (type.indexOf('E') !== -1) {
      t.endianess = true;
    }
    switch (c) {
      case 'd':
        t.cmd = 'e cfg.bigendian=' + t.endianess + ';' + 'pad ' + expect;
        t.expect = asm;
        t.name = filename + ': ' + expect + ' => "' + asm + '"' + colors.blue(' (disassemble)');
        tests.push(t);
        break;
      case 'a':
        t.cmd = 'e cfg.bigendian=' + t.endianess + ';' + 'pa ' + asm;
        t.expect = expect;
        t.name = filename + ': "' + asm + '" => ' + expect + colors.blue(' (assemble)');
        tests.push(t);
        break;
      default:
        continue;
    }
    if (type.indexOf('B') !== -1) {
      t.broken = true;
    }
  }
  return tests;
}

function debase64 (msg) {
  return Buffer.from(msg, 'base64').toString('utf8');
}

function base64 (msg) {
  return Buffer.from(msg).toString('base64');
}

module.exports = NewRegressions;
