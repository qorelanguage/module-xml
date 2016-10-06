#!/usr/bin/env qore

%requires xml
%requires Qorize
%requires ../qlib/WSDL.qm
%new-style

/*
./wsdl-test.q qore_msg -v MaterialMovementInterface_v13.wsdl msg exportMaterialMovementData response -f human

/home/tma/omq/wsdl/msepl_at/wincash/
BasketInterface_v13.wsdl
DbodInterface_v13.wsdl
EmployeeInterface_v13.wsdl
MaterialMovementInterface_v13.wsdl
SalesDataInterface_v13.wsdl

/home/tma/work/qore/module-xml/examples/wsdl-viewer/test/TimeService.wsdl

*/

%exec-class WSDLTools
class WSDLTools {
    private {
        const MSG_REQUEST = 0x1;
        const MSG_RESPONSE = 0x2;
        hash opts = (
            'help': 'h',
            'verbose': 'v:i+',
            'output': 'o=s',
        );
        hash operations = (
            'report': (
            ),
            'msg': (
                'opts': (
                    'format': 'f=s',
                    'comments': 'c',
                    'choices': 'm',
                    'max_items': 'n=s',
                ),
                'params': (
                    True,
                    ('both': MSG_REQUEST|MSG_RESPONSE, 'request': MSG_REQUEST, 'response': MSG_RESPONSE, ),
                ),
                'formats': ('human': True, 'xml': True, 'qore': True, ),
            ),
            'dump': (
                'params': (
                    ('ws': 'ws', 'wsdl': 'wsdl', ),
                ),
            ),
        );
        int verbose = 0;
        WebService ws;
        WSDL::WSMessageHelper wh;
    }

    constructor () {
        my GetOpt g(opts);
        # first try to get general options (help, etc.), do not modify ARGV
        *softlist argv2 = ARGV;
        my hash opt = g.parse(\ARGV);
        my *list objs;
        my string operation;
        my string wsdl_file;
        my softlist params;
        try {
            if (opt.help) {
                help();
            }
            verbose = opt.verbose ?? verbose;
            if (ARGV.empty()) {
                throw "WSDL-TOOL-ERROR", "No WSDL file provided";
            }
            wsdl_file = shift ARGV;

            if (ARGV.empty()) {
                operation = (keys operations)[0];
            } else {
                operation = shift ARGV;
                if (!exists operations{operation}) {
                    throw "WSDL-TOOL-ERROR", sprintf("Operation %y is not in the list of supported operations %y", operation, keys operations);
                }
            }
            if (operations{operation}.opts) {
                g = new GetOpt(opts + operations{operation}.opts);
            }
            # check all options
            ARGV = argv2;
            opt = g.parse3(\ARGV);
            # it might be tricky as changes options may change eaten params
            shift ARGV;  # remove wdsl
            if (!ARGV.empty()) {
                shift ARGV;  # remove operation
            }

            int n = exists operations{operation}.params ? operations{operation}.params.size() : 0;
            if (ARGV.size()-2 > n) {
                throw "WSDL-TOOL-ERROR", sprintf("Operation %y supports up to %y parameters but provided %y", operation, n, ARGV);
            }
            for (my i = 0; i < n; i++) {
                if (operations{operation}.params[i].typeCode() == NT_HASH) {
                    if (i >= ARGV.size()) {
                        push params, operations{operation}.params[i].firstValue();
                    } else {
                        if (exists operations{operation}.params[i]{ARGV[i]}) {
                            push params, operations{operation}.params[i]{ARGV[i]};
                        } else {
                            throw "WSDL-TOOL-ERROR", sprintf("Wrong parameter value %y supported values %y", ARGV[i], keys operations{operation}.params[i]);
                        }
                    }
                } else {
                    push params, ARGV[i] ?? '';
                }
            }
            switch (operation) {
            case 'report':
                break;
            case 'msg':
                if (opt.format) {
                    if (! exists operations{operation}.formats{opt.format}) {
                        throw "WSDL-TOOL-ERROR", sprintf("Value %y is not in the list of supported formats %y\n", opt.format, keys operations{operation}.formats);
                    }
                } else {
                    opt.format = operations{operation}.formats.firstKey();
                }
                break;
            case 'dump':
                break;
            }

        } catch (e) {
            if (e.type == Qore::ET_User) {
                stderr.printf("%s\n\n", e.desc);
                help();
            } else {
                rethrow;
            }
        }
        *hash ws_opts;
        my *string wsdlContent;
        if (wsdl_file == '-') {
            wsdlContent = stdin.read(-1);
        } else {
            string url = wsdl_file;
            if (! (url =~ /:\/\//)) {
                url = 'file://'+url;
                ws_opts = ("def_path": dirname(wsdl_file));
            }
            else {
                string dir = dirname(wsdl_file);
                dir =~ s/^.*:\/\///;
                ws_opts = ("def_path": dir);
            }

            info(sprintf("getFileFromURL(%y)\n", url));
            hash h = Qore::parse_url(url);
            wsdlContent = WSDL::WSDLLib::getFileFromURL(url, h, 'file', NOTHING, NOTHING, True);
        }

        my File output = stdout;
        if (exists opt.output) {
            output = new File();
            output.open(opt.output, O_CREAT | O_TRUNC | O_WRONLY);
        }

        if (operation == 'dump' && params[0] == 'wsdl') {
            output.print(wsdlContent);
            return;
        }

        ws = new WSDL::WebService(wsdlContent, ws_opts);

        if (operation == 'dump' && params[0] == 'ws') {
            ws.wsdl = '';
            output.printf("%N\n", ws);
            return;
        }
        hash o = (
            "comments": opt.comments,
            "choices": opt.choices,
        );
        if (opt.max_items != '') {
            o.max_items = int(opt.max_items);
        }
        wh = new WSDL::WSMessageHelper(ws, o);
        switch (operation) {
            case 'report':
                info(sprintf("%s()\n", operation));
                output.print(ws.getReport(wsdl_file));
                break;
            case 'msg': {
                my list names;
                if (params[0] && params[0] != '-') {
                    names = params[0].split(',');
                } else {
                    names = keys ws.opmap;
                }
                info(sprintf("%s(%y,%y,%y,%y)\n", operation, names, params[1], opt.format, o));
                foreach my string name in (names) {
                    if (!ws.opmap{name}) {
                        throw "WSDL-TOOL-ERROR", sprintf("Value %y is not in the list of operations %y\n", name, keys ws.opmap);
                    }
                    my WSDL::WSOperation op = ws.opmap{name};
                    foreach my int request in ((MSG_REQUEST, MSG_RESPONSE)) {
                        if ((request & params[1]) != 0 && ((request==MSG_REQUEST) ? op.input : op.output)) {
                            my hash msg = wh.getMessage((request==MSG_REQUEST) ? op.input : op.output);
                            switch (opt.format) {
                            case 'xml':
                                if (!opt.choices) {
                                    info("Only the first choice is considered\n");
                                }
                                output.print(request == MSG_REQUEST ?
                                    op.serializeRequest(msg, NOTHING, NOTHING, NOTHING, NOTHING, XGF_ADD_FORMATTING).body :
                                    op.serializeResponse(msg, NOTHING, NOTHING, NOTHING, NOTHING, XGF_ADD_FORMATTING).body
                                );
                                break;
                            case 'qore':
                                output.print(qorize(msg, name, True));
                                break;
                            case 'human':
                                output.printf("%s: %N\n", name, msg);
                                break;
                            }
                            output.print("\n\n");
                        }
                    }
                }
            }
        }
    }

    private help() {
        stderr.printf(
            "WSDL tools, ver 0.1\n"
            "usage: %s [options] [-o <output>] [-f <format>] <wsdl> [<cmd> [params]]\n"
            "  options\n"
            "  -v     verbose\n"
            "  -h     help\n"
            "  wsdl   WSDL file or URL. If '-' specified then expected at STDIN\n"
            "  output unless file specified then goes to STDOUT\n"
            "\n"
            "Commands:\n"
            "  report\n"
            "    report supported services, operations, etc.\n"
            "\n"
            "  msg [<operation> [both|request|response]]\n"
            "    print request or response message related to operation\n"
            "    operation    unless specified (or '-') print all operations\n"
            "    format:\n"
            "      human  human-readable, default\n"
            "      qore   Qore source file\n"
            "      xml    XML SOAP message\n"
            "    options:\n"
            "      -c     output comments\n"
            "      -m     output multiple choices\n"
            "      -n <n> max.array elements, default: 3\n"
            "\n"
            "  dump <what>\n"
            "    dump specified information\n"
            "    What:\n"
            "      wsdl   print WSDL on output\n"
            "      ws     print WebService on output\n"
            "\n"
            "\n"
            "Example:\n"
            "  %s MaterialMovementInterface_v13.wsdl msg exportMaterialMovementData request -f qore\n"
            "\n"
        ,
            get_script_name(),
            get_script_name(),
        );
        exit(1);
    }

    private info(string s) { # , any *p1, any *p2, any *p3, any *p4, any *p5) {  # TODO: elipsis params and vprintf()
        if (verbose) {
            stderr.print(s);
            #stderr.vprintf(s, p1, p2, p3, p4, p5);
        }
    }
}
