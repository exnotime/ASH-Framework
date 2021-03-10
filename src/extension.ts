// The module 'vscode' contains the VS Code extensibility API
// Import the module and reference it with the alias vscode in your code below
import * as vscode from 'vscode';
import { WorkspaceFolder, DebugConfiguration, ProviderResult, CancellationToken } from 'vscode';
import script_interface from './interface.json';
import { ASTaskProvider } from './asTaskProvider';
import * as cp from 'child_process';
import {ASDebugSession} from './asDebugAdapter';
import * as Net from 'net';
import { networkInterfaces } from 'os';
let asTaskProvider: vscode.Disposable | undefined;
// this method is called when your extension is activated
// your extension is activated the very first time the command is executed
export function activate(context: vscode.ExtensionContext) {

	let globalProvider = vscode.languages.registerCompletionItemProvider({scheme:'file', language:'angelscript'}, {

		provideCompletionItems(document: vscode.TextDocument, position: vscode.Position, token: vscode.CancellationToken, context: vscode.CompletionContext) {
			let completions : Array<vscode.CompletionItem> = Array<vscode.CompletionItem>(0);

			let settings = vscode.workspace.getConfiguration('angelscript_helper');
			let interfaceFile = settings['interfaceLocation'];
			if(interfaceFile === ''){
				return null;
			}
			var fs = require('fs');
			var interfaceObject;
			var interfaceJSON = fs.readFileSync(interfaceFile);
			interfaceObject = JSON.parse(interfaceJSON);

			interfaceObject.GlobalFunctions.forEach(func => {
				let comp = new vscode.CompletionItem(func.Name, vscode.CompletionItemKind.Function);
				comp.detail = func.ReturnType + " " + func.Name + "(";
				func.Params.forEach(param => {
					comp.detail += param.Type + " " + param.Name + ", ";
				});
				comp.detail += ")";
				comp.commitCharacters = ['('];
				completions.push(comp);
			});

			interfaceObject.GlobalTypes.forEach(type => {
				let comp = new vscode.CompletionItem(type.Name, vscode.CompletionItemKind.Class);
				comp.commitCharacters = ['('];
				completions.push(comp);
			});

			interfaceObject.GlobalEnums.forEach(e => {
				let comp = new vscode.CompletionItem(e.Name, vscode.CompletionItemKind.Enum);
				comp.commitCharacters = ['::'];
				completions.push(comp);
			});


			let moduleCache = settings['moduleCacheDir'];
			if(moduleCache){
				var path = require('path');
				let cachedModuleFile = moduleCache + path.parse(document.fileName).name + '.json';
				let moduleJSON = fs.readFileSync(cachedModuleFile);
				let moduleObject = JSON.parse(moduleJSON);

				moduleObject.Functions.forEach(func => {
					let comp = new vscode.CompletionItem(func.Name, vscode.CompletionItemKind.Function);
					comp.detail = func.ReturnType + " " + func.Name + "(";
					func.Params.forEach(param => {
						comp.detail += param.Type + " " + param.Name + ", ";
					});
					comp.detail += ")";
					comp.commitCharacters = ['('];
					completions.push(comp);
				});
	
				moduleObject.Types.forEach(type => {
					let comp = new vscode.CompletionItem(type.Name, vscode.CompletionItemKind.Class);
					comp.commitCharacters = ['('];
					completions.push(comp);
				});
	
				moduleObject.Enums.forEach(e => {
					let comp = new vscode.CompletionItem(e.Name, vscode.CompletionItemKind.Enum);
					comp.commitCharacters = ['::'];
					completions.push(comp);
				});

				moduleObject.GlobalVariables.forEach(v => {
					let comp = new vscode.CompletionItem(v.Name, vscode.CompletionItemKind.Variable);
					comp.commitCharacters = [];
					completions.push(comp);
				});
			}

			completions.push({label:'for',kind:vscode.CompletionItemKind.Function});
			completions.push({label:'if',kind:vscode.CompletionItemKind.Function});
			completions.push({label:'else',kind:vscode.CompletionItemKind.Function});
			completions.push({label:'elseif',kind:vscode.CompletionItemKind.Function});
			completions.push({label:'is',kind:vscode.CompletionItemKind.Function});
			completions.push({label:'do',kind:vscode.CompletionItemKind.Function});
			completions.push({label:'while',kind:vscode.CompletionItemKind.Function});

			completions.push({label:'int',kind:vscode.CompletionItemKind.Variable});
			completions.push({label:'int8',kind:vscode.CompletionItemKind.Variable});
			completions.push({label:'int16',kind:vscode.CompletionItemKind.Variable});
			completions.push({label:'int64',kind:vscode.CompletionItemKind.Variable});
			completions.push({label:'uint',kind:vscode.CompletionItemKind.Variable});
			completions.push({label:'uint8',kind:vscode.CompletionItemKind.Variable});
			completions.push({label:'uint16',kind:vscode.CompletionItemKind.Variable});
			completions.push({label:'uint64',kind:vscode.CompletionItemKind.Variable});

			completions.push({label:'float',kind:vscode.CompletionItemKind.Variable});
			completions.push({label:'double',kind:vscode.CompletionItemKind.Variable});
			completions.push({label:'bool',kind:vscode.CompletionItemKind.Variable});

			// return all completion items as array
			return completions;
		}
	});


	//BackwardIterator taken from the shader code extension
	const _NL = '\n'.charCodeAt(0);
	const _TAB = '\t'.charCodeAt(0);
	const _WSB = ' '.charCodeAt(0);
	const _LBracket = '['.charCodeAt(0);
	const _RBracket = ']'.charCodeAt(0);
	const _LCurly = '{'.charCodeAt(0);
	const _RCurly = '}'.charCodeAt(0);
	const _LParent = '('.charCodeAt(0);
	const _RParent = ')'.charCodeAt(0);
	const _Comma = ','.charCodeAt(0);
	const _Quote = '\''.charCodeAt(0);
	const _DQuote = '"'.charCodeAt(0);
	const _USC = '_'.charCodeAt(0);
	const _a = 'a'.charCodeAt(0);
	const _z = 'z'.charCodeAt(0);
	const _A = 'A'.charCodeAt(0);
	const _Z = 'Z'.charCodeAt(0);
	const _0 = '0'.charCodeAt(0);
	const _9 = '9'.charCodeAt(0);

	const BOF = 0;

	class BackwardIterator {
		private lineNumber: number;
		private offset: number;
		private line: string;
		private model: vscode.TextDocument;

		constructor(model: vscode.TextDocument, offset: number, lineNumber: number) {
			this.lineNumber = lineNumber;
			this.offset = offset;
			this.line = model.lineAt(this.lineNumber).text;
			this.model = model;
		}

		public hasNext(): boolean {
			return this.lineNumber >= 0;
		}

		public next(): number {
			if (this.offset < 0) {
				if (this.lineNumber > 0) {
					this.lineNumber--;
					this.line = this.model.lineAt(this.lineNumber).text;
					this.offset = this.line.length - 1;
					return _NL;
				}
				this.lineNumber = -1;
				return BOF;
			}
			let ch = this.line.charCodeAt(this.offset);
			this.offset--;
			return ch;
		}
	}

	class ForwardIterator {
		private lineNumber: number;
		private offset: number;
		private line: string;
		private model: vscode.TextDocument;

		constructor(model: vscode.TextDocument, offset: number, lineNumber: number) {
			this.lineNumber = lineNumber;
			this.offset = offset;
			this.line = model.lineAt(this.lineNumber).text;
			this.model = model;
		}

		public hasNext(): boolean {
			return this.lineNumber < this.model.lineCount;
		}

		public next(): number {
			if (this.offset > this.line.length) {
				if (this.lineNumber < this.model.lineCount) {
					this.lineNumber++;
					this.line = this.model.lineAt(this.lineNumber).text;
					this.offset = 0;
					return _NL;
				}
				this.lineNumber = -1;
				return BOF;
			}
			let ch = this.line.charCodeAt(this.offset);
			this.offset++;
			return ch;
		}
	}

	function readArguments(iterator: BackwardIterator): number {
        let parentNesting = 0;
        let bracketNesting = 0;
        let curlyNesting = 0;
        let paramCount = 0;
        while (iterator.hasNext()) {
            let ch = iterator.next();
            switch (ch) {
                case _LParent:
                    parentNesting--;
                    if (parentNesting < 0) {
                        return paramCount;
                    }
                    break;
                case _RParent: parentNesting++; break;
                case _LCurly: curlyNesting--; break;
                case _RCurly: curlyNesting++; break;
                case _LBracket: bracketNesting--; break;
                case _RBracket: bracketNesting++; break;
                case _DQuote:
                case _Quote:
                    while (iterator.hasNext() && ch !== iterator.next()) {
                        // find the closing quote or double quote
                    }
                    break;
                case _Comma:
                    if (!parentNesting && !bracketNesting && !curlyNesting) {
                        paramCount++;
                    }
                    break;
            }
        }
        return -1;
    }

    function isIdentPart(ch: number): boolean {
        if (ch === _USC || // _
            ch >= _a && ch <= _z || // a-z
            ch >= _A && ch <= _Z || // A-Z
            ch >= _0 && ch <= _9 || // 0/9
            ch >= 0x80 && ch <= 0xFFFF) { // nonascii

            return true;
        }
        return false;
    }

    function readIdent(iterator: BackwardIterator | ForwardIterator): string {
        let identStarted = false;
        let ident = '';
        while (iterator.hasNext()) {
            let ch = iterator.next();
            if (!identStarted && (ch === _WSB || ch === _TAB || ch === _NL)) {
                continue;
            }
            if (isIdentPart(ch)) {
                identStarted = true;
                ident = String.fromCharCode(ch) + ident;
            } else if (identStarted) {
                return ident;
            }
        }
        return ident;
	}

	let functionSignatureProvider = vscode.languages.registerSignatureHelpProvider({scheme:'file', language:'angelscript'}, {
		provideSignatureHelp(document: vscode.TextDocument, position: vscode.Position, token: vscode.CancellationToken, context: vscode.SignatureHelpContext){
			let iterator = new BackwardIterator(document, position.character - 1, position.line);
			let paramCount = readArguments(iterator);
			if(paramCount < 0){
				return null;
			}
			let ident = readIdent(iterator);
			if (!ident) {
				return null;
			}
			let settings = vscode.workspace.getConfiguration('angelscript_helper');
			let interfaceFile = settings['interfaceLocation'];
			if(interfaceFile === ''){
				return null;
			}
			var fs = require('fs');
			var interfaceObject;
			var interfaceJSON = fs.readFileSync(interfaceFile);
			interfaceObject = JSON.parse(interfaceJSON);
			//find function
			let signHelp = new vscode.SignatureHelp();
			let i = 0;
			interfaceObject.GlobalFunctions.forEach(func => {
				if(func.Name === ident && paramCount < func.Params.length){
					let signature = func.ReturnType + ' ' + ident;
					let infos : vscode.ParameterInformation[] = [];
					signature += '(';
					let params = '';
					func.Params.forEach( p => {
						params += p.Type + ' ' + p.Name;
						if(p.DefaultVal){
							params += ' = ' + p.DefaultVal;
						}
						params += ', ';
						infos.push({label: p.Name,});
					});
					signature += params.slice(0, -2);
					signature += ')';
					let signInfo = new vscode.SignatureInformation(signature);
					signInfo.parameters = infos;
					signHelp.signatures.push(signInfo);
					i++;
				}
			});
			signHelp.activeSignature = 0;
			signHelp.activeParameter = Math.min(paramCount, signHelp.signatures[0].parameters.length - 1);
			return signHelp;
		}
	},'(',',');

	let enumCompletetionProvider = vscode.languages.registerCompletionItemProvider({scheme:'file', language:'angelscript'}, {

		provideCompletionItems(document: vscode.TextDocument, position: vscode.Position, token: vscode.CancellationToken, context: vscode.CompletionContext) {
			let completions : Array<vscode.CompletionItem> = Array<vscode.CompletionItem>(0);

			let iterator = new BackwardIterator(document, position.character - 1, position.line);
			let ident = readIdent(iterator);
			if (!ident) {
				return null;
			}

			let settings = vscode.workspace.getConfiguration('angelscript_helper');
			let interfaceFile = settings['interfaceLocation'];
			if(interfaceFile === ''){
				return null;
			}
			var fs = require('fs');
			var interfaceObject;
			var interfaceJSON = fs.readFileSync(interfaceFile);
			interfaceObject = JSON.parse(interfaceJSON);

			interfaceObject.GlobalEnums.forEach(e => {
				if(e.Name === ident){
					e.Values.forEach(v => {
						let comp = new vscode.CompletionItem(v.Name, vscode.CompletionItemKind.EnumMember);
						completions.push(comp);
					});
				}
			});
			
			// return all completion items as array
			return completions;
		}
	},':');


	let variableCompletetionProvider = vscode.languages.registerCompletionItemProvider({scheme:'file', language:'angelscript'}, {

		provideCompletionItems(document: vscode.TextDocument, position: vscode.Position, token: vscode.CancellationToken, context: vscode.CompletionContext) {
			let completions : Array<vscode.CompletionItem> = Array<vscode.CompletionItem>(0);
			let settings = vscode.workspace.getConfiguration('angelscript_helper');
			let moduleCache = settings['moduleCacheDir'];
			if(moduleCache){

				let iterator = new BackwardIterator(document, position.character - 1, position.line);
				let ident = readIdent(iterator);
				if (!ident) {
					return null;
				}

				var path = require('path');
				var fs = require('fs');
				//Cache in memory somewhere?
				let cachedModuleFile = moduleCache + path.parse(document.fileName).name + '.json';
				let moduleJSON = fs.readFileSync(cachedModuleFile);
				let moduleObject = JSON.parse(moduleJSON);
				//Find type
				var type;
				for(let v of moduleObject.GlobalVariables){
					if(v.Name === ident){
						type = v.Type;
						break;
					}
				}
				//Find type information
				for(let t of moduleObject.Types){
					if(t.Name === type){
						for(let p of t.Properties){
							let comp = new vscode.CompletionItem(p.Name, vscode.CompletionItemKind.Property);
							completions.push(comp);
						}
						break;
					}
				}
				
				for(let f of moduleObject.Functions){
					if(position.line > f.StartLine && position.line < f.EndLine){
						for(let v of f.Variables){
							let comp = new vscode.CompletionItem(v.Name, vscode.CompletionItemKind.Variable);
							completions.push(comp);
						}
						for(let p of f.Params){
							let comp = new vscode.CompletionItem(p.Name, vscode.CompletionItemKind.Variable);
							completions.push(comp);
						}
					}

				}

			}
			
			// return all completion items as array
			return completions;
		}
	},'.');

	let workspaceRoot = vscode.workspace.rootPath;
	if (!workspaceRoot) {
		return;
	}
	asTaskProvider = vscode.tasks.registerTaskProvider(ASTaskProvider.asType, new ASTaskProvider(workspaceRoot));
	function exec(command: string, options: cp.ExecOptions): Promise<{ stdout: string; stderr: string }> {
		return new Promise<{ stdout: string; stderr: string }>((resolve, reject) => {
			cp.exec(command, options, (error, stdout, stderr) => {
				if (error) {
					reject({ error, stdout, stderr });
				}
				resolve({ stdout, stderr });
			});
		});
	}

	const diagCollection = vscode.languages.createDiagnosticCollection('ash');

	const buildDiagnostics = async function(ashLocation:string, moduleCache:string, interfaceFile:string, file:string){
		let commandLine = ashLocation + ' -c -s ' + file + ' -i ' + interfaceFile;
		if(moduleCache){
			var path = require('path');
			commandLine += ' -r ' + moduleCache + path.parse(file).name + '.json';
		}
		let { stdout, stderr } = await exec(commandLine, { });
		if(stderr){
			return;
		}
		diagCollection.clear();
		if(stdout){
			if(stdout === 'Missing source file'){
				return;
			}
			let resultObjects = JSON.parse(stdout);
			if(resultObjects){
				let sectionProblems : Map<string, vscode.Diagnostic[]> = new Map<string, vscode.Diagnostic[]>();
				resultObjects.Warnings.forEach(w => {
					let line = w.Line - 1 < 0 ? 0 : w.Line - 1;
					let column = w.Column - 1 < 0 ? 0 : w.Column - 1;
					let range = new vscode.Range(new vscode.Position(line, column), new vscode.Position(line, column + 1));
					let currentDocument = vscode.Uri.file(w.Section);
					vscode.workspace.openTextDocument(currentDocument).then((doc:vscode.TextDocument) => {
						if(doc){
							let ident = readIdent(new ForwardIterator(doc, column, line));
							range = new vscode.Range(range.start, new vscode.Position(line, column + ident.length));
						}
					});
					let diag = new vscode.Diagnostic(range, w.Message, vscode.DiagnosticSeverity.Warning);
					let location = new vscode.Location(w.Section, range);
					diag.relatedInformation?.push(new vscode.DiagnosticRelatedInformation(location, w.Message));
					if(!sectionProblems.has(w.Section)){
						sectionProblems.set(w.Section, []);
					}
					let problems = sectionProblems.get(w.Section);
					if(problems){
						problems.push(diag);
					}
				});

				resultObjects.Errors.forEach(e => {
					let line = e.Line - 1 < 0 ? 0 : e.Line - 1;
					let column = e.Column - 1 < 0 ? 0 : e.Column - 1;
					let range = new vscode.Range(new vscode.Position(line, column), new vscode.Position(line, column + 1));
					let currentDocument = vscode.Uri.file(e.Section);
					vscode.workspace.openTextDocument(currentDocument).then((doc:vscode.TextDocument) => {
						if(doc){
							let ident = readIdent(new ForwardIterator(doc, column, line));
							range = new vscode.Range(range.start, new vscode.Position(line, column + ident.length));
						}
					});
					let diag = new vscode.Diagnostic(range, e.Message, vscode.DiagnosticSeverity.Error);
					let location = new vscode.Location(e.Section, range);
					diag.relatedInformation?.push(new vscode.DiagnosticRelatedInformation(location, e.Message));
					if(!sectionProblems.has(e.Section)){
						sectionProblems.set(e.Section, []);
					}
					let problems = sectionProblems.get(e.Section);
					if(problems){
						problems.push(diag);
					}
				});

				for(let entry of sectionProblems.entries()){
					diagCollection.set( vscode.Uri.file(entry[0]), entry[1]);
				}
			}
		}
	};

	const buildProjectCommand = async function() {
		let settings = vscode.workspace.getConfiguration('angelscript_helper');
		let interfaceFile = settings['interfaceLocation'];
		if(interfaceFile === ''){
			return;
		}
		let ashLocation = settings['ashExeLocation'];
		if(ashLocation === ''){
			return;
		}
		let moduleCache = settings['moduleCacheDir'];

		let sourceFile = settings['sourceFile'];
		if(sourceFile === ''){
			return;
		}
		buildDiagnostics(ashLocation, moduleCache, interfaceFile, sourceFile);
	};

	const buildFileCommand  = async function() {
		let settings = vscode.workspace.getConfiguration('angelscript_helper');
		let interfaceFile = settings['interfaceLocation'];
		if(interfaceFile === ''){
			return;
		}
		let ashLocation = settings['ashExeLocation'];
		if(ashLocation === ''){
			return;
		}
		let moduleCache = settings['moduleCacheDir'];
		let file = vscode.window.activeTextEditor?.document.fileName;
		if(file){
			buildDiagnostics(ashLocation, moduleCache, interfaceFile, file);
		}
	};

	context.subscriptions.push(vscode.commands.registerCommand('extension.ash_build_project', buildProjectCommand));
	context.subscriptions.push(vscode.commands.registerCommand('extension.ash_build_current_file', buildFileCommand));
	function buildOnEvent(document:vscode.TextDocument | undefined){
		if (document && vscode.workspace.getWorkspaceFolder(document.uri)) {
			let settings = vscode.workspace.getConfiguration('angelscript_helper');
			let interfaceFile = settings['interfaceLocation'];
			if(interfaceFile === ''){
				return;
			}
			let ashLocation = settings['ashExeLocation'];
			if(ashLocation === ''){
				return;
			}
			let moduleCache = settings['moduleCacheDir'];
			let file = document.fileName;
			if(file){
				buildDiagnostics(ashLocation, moduleCache, interfaceFile, file);
			}
		}
	}

	context.subscriptions.push(vscode.window.onDidChangeActiveTextEditor(editor => {buildOnEvent(editor?.document); }));
	context.subscriptions.push(vscode.workspace.onDidSaveTextDocument(document => {buildOnEvent(document); }));

	//Debugger
	context.subscriptions.push(vscode.commands.registerCommand('extension.ash_get_program_name', config => {
		return vscode.window.showInputBox({
			placeHolder: "Please enter the location of a angelscript program to launch",
			value: "enter program here..."
		});
	}));

	const provider = new ASConfigurationProvider();
	context.subscriptions.push(vscode.debug.registerDebugConfigurationProvider('ASH', provider));

	let factory: vscode.DebugAdapterDescriptorFactory = new ASDebugAdapterDescriptorFactory();
	context.subscriptions.push(vscode.debug.registerDebugAdapterDescriptorFactory('ASH', factory));
	if ('dispose' in factory) {
		context.subscriptions.push(factory);
	}
}

// this method is called when your extension is deactivated
export function deactivate() {
	if(asTaskProvider){
		asTaskProvider.dispose();
	}
}

class ASConfigurationProvider implements vscode.DebugConfigurationProvider {

	/**
	 * Massage a debug configuration just before a debug session is being launched,
	 * e.g. add all missing attributes to the debug configuration.
	 */
	resolveDebugConfiguration(folder: WorkspaceFolder | undefined, config: DebugConfiguration, token?: CancellationToken): ProviderResult<DebugConfiguration> {

		// if launch.json is missing or empty
		if (!config.type && !config.request && !config.name) {
			const editor = vscode.window.activeTextEditor;
			if (editor && editor.document.languageId === 'angelscript') {
				config.type = 'ASH';
				config.name = 'Launch';
				config.request = 'launch';
				config.program = '${file}';
			}
		}

		if (!config.program) {
			return vscode.window.showInformationMessage("Cannot find a program to debug").then(_ => {
				return undefined;	// abort launch
			});
		}

		return config;
	}
}

class ASDebugAdapterDescriptorFactory implements vscode.DebugAdapterDescriptorFactory {

	private server?: Net.Server;

	createDebugAdapterDescriptor(session: vscode.DebugSession, executable: vscode.DebugAdapterExecutable | undefined): vscode.ProviderResult<vscode.DebugAdapterDescriptor> {

		if (!this.server) {
			// start listening on a random port
			this.server = Net.createServer(socket => {
				const session = new ASDebugSession();
				session.setRunAsServer(true);
				session.start(<NodeJS.ReadableStream>socket, socket);
			}).listen(0);
		}

		// make VS Code connect to debug server
		return new vscode.DebugAdapterServer((<Net.AddressInfo>this.server.address()).port);
	}

	dispose() {
		if (this.server) {
			this.server.close();
		}
	}
}
