import {
	Logger, logger,
	LoggingDebugSession,
	InitializedEvent, TerminatedEvent, StoppedEvent, BreakpointEvent, OutputEvent,
	ProgressStartEvent, ProgressUpdateEvent, ProgressEndEvent,
	Thread, StackFrame, Scope, Source, Handles, Breakpoint, Variable
} from 'vscode-debugadapter';


import { DebugProtocol } from 'vscode-debugprotocol';
import { basename } from 'path';
import { fstat } from 'fs';
import * as fs from 'fs';
import { exec, execFile, spawn, ChildProcess } from 'child_process';
import { rejects } from 'assert';
import net = require('net'); 
import { downloadAndUnzipVSCode } from 'vscode-test';
import * as vscode from 'vscode';
import { Debugger } from 'inspector';
//import { MockRuntime, MockBreakpoint } from './mockRuntime';
const { Subject } = require('await-notify');

function timeout(ms: number) {
	return new Promise(resolve => setTimeout(resolve, ms));
}

interface LaunchRequestArgs extends DebugProtocol.LaunchRequestArguments{
	program:string;
	workingdir:string;
	address:string;
	port:number;
	arguments:string;
}

class AsBreakpoint{
	Line:number;
	ID:number;
	constructor(){
		this.Line = 0;
		this.ID = 0;
	}
};

export class ASDebugSession extends LoggingDebugSession {
    private static THREAD_ID = 1;

    // a Mock runtime (or debugger)
	//private _runtime: MockRuntime;

	private _variableHandles = new Handles<string>();
	private _handleMap = new Map<number,number>();
	private _configurationDone = new Subject();

	private _cancelationTokens = new Map<number, boolean>();
	private _isLongrunning = new Map<number, boolean>();
	private _breakpoints = new Map<string, DebugProtocol.Breakpoint[]>();
	private _reportProgress = false;
	private _progressId = 10000;
	private _cancelledProgressId: string | undefined = undefined;
    private _isProgressCancellable = true;
	private _connection!: DebuggerConnection;
	private _process!: ChildProcess;
	private _ids = 0;
	private _line : number;
	private _callstack : JSON | undefined;
	private _global_vars : JSON | undefined;
	private _variables : Array<JSON> = [];
	private _breakpointMessages : [] = [];
    public constructor(){
		super('ASDebugger.txt');
		this.setDebuggerLinesStartAt1(true);
		this.setDebuggerColumnsStartAt1(true);
		this._line = 0;
	}
	
	protected initializeRequest(response: DebugProtocol.InitializeResponse, args: DebugProtocol.InitializeRequestArguments): void {

		if (args.supportsProgressReporting) {
			this._reportProgress = true;
		}

		// build and return the capabilities of this debug adapter:
		response.body = response.body || {};

		// the adapter implements the configurationDoneRequest.
		response.body.supportsConfigurationDoneRequest = true;

		// make VS Code to use 'evaluate' when hovering over source
		response.body.supportsEvaluateForHovers = true;
		// make VS Code to support data breakpoints
		response.body.supportsDataBreakpoints = true;
		response.body.supportsTerminateRequest = true;

		// make VS Code to send cancelRequests
		response.body.supportsCancelRequest = true;

		// make VS Code send the breakpointLocations request
		response.body.supportsBreakpointLocationsRequest = true;

		response.body.supportsConditionalBreakpoints = true;

		this.sendResponse(response);

		// since this debug adapter can accept configuration requests like 'setBreakpoint' at any time,
		// we request them early by sending an 'initializeRequest' to the frontend.
		// The frontend will end the configuration sequence by calling 'configurationDone' request.
		this.sendEvent(new InitializedEvent());
	}

	protected configurationDoneRequest(response: DebugProtocol.ConfigurationDoneResponse, args: DebugProtocol.ConfigurationDoneArguments): void {
		super.configurationDoneRequest(response, args);

		// notify the launchRequest that configuration has finished
		this._configurationDone.notify();
	}

	protected async launchRequest(response: DebugProtocol.LaunchResponse, args: LaunchRequestArgs) {
		// wait until configuration has finished (and configurationDoneRequest has been called)
		await this._configurationDone.wait(1000);

		// start the program
		let shellCmd = args.program;
		this._process = spawn(shellCmd, [args.arguments], {cwd:args.workingdir});
		const channel = vscode.window.createOutputChannel(args.program);
		this._process.stdout?.on('data', (data) => {
			channel.appendLine(data.toString());
			console.log(data.toString());
		});
		this._process.on('close', (code, signal) => {
			this.sendEvent(new TerminatedEvent());
		});
		console.log("Starting process");
		this._connection = new DebuggerConnection(args.address, args.port, this);

		for(let b of this._breakpointMessages){
			this._connection.Send(b);
		}
		var finishedSetupMsg = {
			Type:"FinishedSetup"
		};
		this._connection.Send(JSON.stringify(finishedSetupMsg));
		this.sendResponse(response);
	}

	public onData(data:string) {
		let jsonMsg = JSON.parse(data);
		if(jsonMsg.Type === 'HitBreakpoint'){
			this._line = jsonMsg.Line;
			this._callstack = jsonMsg.CallStack;
			this._global_vars = jsonMsg.GlobalVars;
			this.sendEvent(new StoppedEvent('breakpoint', ASDebugSession.THREAD_ID));
		} else if (jsonMsg.Type === 'ValidatedBreakpoint'){
			this.sendEvent(new BreakpointEvent('changed', <DebugProtocol.Breakpoint>{verified:true, id:jsonMsg.ID}));
		}else if(jsonMsg.Type === 'Step'){
			this._line = jsonMsg.Line;
			this._callstack = jsonMsg.CallStack;
			this._global_vars = jsonMsg.GlobalVars;
			this.sendEvent(new StoppedEvent('step', ASDebugSession.THREAD_ID));
		}
	}

	protected cancelRequest(response: DebugProtocol.CancelResponse, args: DebugProtocol.CancelArguments) {
		if (args.requestId) {
			this._cancelationTokens.set(args.requestId, true);
		}
		if (args.progressId) {
			this._cancelledProgressId= args.progressId;
		}
	}

	protected terminateRequest(response: DebugProtocol.TerminateResponse, args: DebugProtocol.TerminateArguments, request?: DebugProtocol.Request): void{
		super.terminateRequest(response, args, request);
		this._process.kill();
		this.sendEvent(new TerminatedEvent());
	}

	protected continueRequest(response: DebugProtocol.ContinueResponse, args: DebugProtocol.ContinueArguments): void {
		var continueMessage = {
			Type:"Continue"
		};
		var jsonMsg = JSON.stringify(continueMessage);
		this._connection.Send(jsonMsg);
		this.sendResponse(response);
	}

	protected pauseRequest(response: DebugProtocol.PauseResponse, args: DebugProtocol.PauseArguments, request?: DebugProtocol.Request): void {
		var pauseMessage = {
			Type:"Pause"
		};
		var jsonMsg = JSON.stringify(pauseMessage);
		this._connection.Send(jsonMsg);
		this.sendResponse(response);
	}

	protected setBreakPointsRequest(response: DebugProtocol.SetBreakpointsResponse, args: DebugProtocol.SetBreakpointsArguments) : void {
		if(!args.breakpoints){
			return;
		}

		let responseBreakpoints : Array<DebugProtocol.Breakpoint> = Array<DebugProtocol.Breakpoint>(0);
		let breakpoints : Array<AsBreakpoint> = Array<AsBreakpoint>(0);
		for(let b of args.breakpoints){
			let breakpoint: AsBreakpoint = new AsBreakpoint();
			breakpoint.Line = this.convertClientLineToDebugger(b.line);
			breakpoint.ID = this._ids++;
			breakpoints.push(breakpoint);

			const bp = <DebugProtocol.Breakpoint> new Breakpoint(false, this.convertClientLineToDebugger(b.line));
			bp.id = breakpoint.ID;
			responseBreakpoints.push(bp);
		}

		var breakpointMessage = {
			Type: "SetBreakpoints",
			File: args.source.path || "",
			Breakpoints: breakpoints
		};
		//change the file name to something that matches what angelscript returns
		breakpointMessage.File = breakpointMessage.File.split('\\').join('/');//replace('\\','/');
		//Set first char to upper
		breakpointMessage.File = breakpointMessage.File.charAt(0).toUpperCase() + breakpointMessage.File.slice(1);

		var jsonBreakpoint = JSON.stringify(breakpointMessage);
		if(this._process){
			this._connection.Send(jsonBreakpoint);
		}else{
			//we have not started the process yet
			this._breakpointMessages.push(jsonBreakpoint);
		}
		this._breakpoints.set(breakpointMessage.File, responseBreakpoints);

		response.body = {
			breakpoints: responseBreakpoints
		};

		this.sendResponse(response);
	}

	protected breakpointLocationsRequest(response: DebugProtocol.BreakpointLocationsResponse, args: DebugProtocol.BreakpointLocationsArguments, request?: DebugProtocol.Request): void {
		if (args.source.path) {
			response.body.breakpoints.push({line:this._line});
		} else {
			response.body = {
				breakpoints: []
			};
		}
		this.sendResponse(response);
	}

	protected threadsRequest(response: DebugProtocol.ThreadsResponse, request?: DebugProtocol.Request): void{
		response.body = { threads: [ new Thread(ASDebugSession.THREAD_ID, "thread 1") ] };
		this.sendResponse(response);
	}

	protected stackTraceRequest(response: DebugProtocol.StackTraceResponse, args: DebugProtocol.StackTraceArguments): void {
		let i : number = 0;
		let stack : StackFrame[] = []; 
		for(let frame of this._callstack){
			stack.push({id:i, line:frame.Line, column:0, name:frame.Declaration, source:this.createSource(frame.File)});
			i++;
		}
		response.body = {
			stackFrames: stack,
			totalFrames: i
		};
		this.sendResponse(response);
	}

	private createSource(filePath: string): Source {
		return new Source(basename(filePath), this.convertDebuggerPathToClient(filePath), undefined, undefined, 'mock-adapter-data');
	}
	private globalScope = 0x145C21CA2;
	protected scopesRequest(response: DebugProtocol.ScopesResponse, args: DebugProtocol.ScopesArguments): void {
		this._variables = []; //clear any existing variables
		let scopes : Scope[] = [];
		let i = 1;
		scopes.push(new Scope("Globals", this.globalScope, false));
		//this._handleMap.set(32, -1);

		for(let frame of this._callstack){
			let id = this._variableHandles.create(frame.Declaration);
			scopes.push(new Scope(frame.Declaration, i, false));
			this._handleMap.set(id, i);
			i++;
		}
		response.body = {
			scopes:scopes
		};
		this.sendResponse(response);
	}

	protected async variablesRequest(response: DebugProtocol.VariablesResponse, args: DebugProtocol.VariablesArguments, request?: DebugProtocol.Request) {
		let variables : Variable[] = [];
		const variableBit = 0x1000;
		let id = args.variablesReference;
		if(id === this.globalScope){
			for(let variable of this._global_vars){
				variables.push(
					{name: variable.Declaration, value: variable.Value.toString(), variablesReference: 0}
				);
			}
		}else{
			if((id & variableBit) === 0){
				for(let variable of this._callstack[id - 1].Variables){
					if(variable.Properties){
						id = this._variables.length;
						this._variables.push(variable);
						id |= variableBit;
					}else{
						id = 0;
					}
					variables.push(
						{name:variable.Declaration, value:variable.Value?.toString(), variablesReference: id}
					);
				}
			}else{
				id = id & (variableBit - 1);
				for(let prop of this._variables[id].Properties){
					let newID = 0;
					if(prop.Properties){
						newID = this._variables.length;
						this._variables.push(prop);
						newID |= variableBit;
					}
					variables.push(
						{name:prop.Declaration, value:prop.Value?.toString(), variablesReference: newID}
					);
				}
			}
		}
		
		response.body = {
			variables: variables
		};
		this.sendResponse(response);
	}

	protected nextRequest(response: DebugProtocol.NextResponse, args: DebugProtocol.NextArguments): void {
		var message = {
			Type:"StepOver"
		};
		var jsonMsg = JSON.stringify(message);
		this._connection.Send(jsonMsg);
		this.sendResponse(response);
	}

	protected stepInRequest(response: DebugProtocol.StepInResponse, args: DebugProtocol.StepInArguments): void {
		var message = {
			Type:"StepIn"
		};
		var jsonMsg = JSON.stringify(message);
		this._connection.Send(jsonMsg);
		this.sendResponse(response);
	}

	protected stepOutRequest(response: DebugProtocol.StepOutResponse, args: DebugProtocol.StepOutArguments): void {
		var message = {
			Type:"StepOut"
		};
		var jsonMsg = JSON.stringify(message);
		this._connection.Send(jsonMsg);
		this.sendResponse(response);
	}
}

class DebuggerConnection{
	m_Socket:net.Socket;
	m_Session:ASDebugSession;

	constructor(ip:string, port:number, session: ASDebugSession){
		this.m_Socket = new net.Socket();
		this.m_Socket.setNoDelay(true);
		this.m_Socket.connect(port, ip);
		this.m_Socket.write('{\"Type\":\"OpenConnection\"}');
		this.m_Socket.write('\n');
		this.m_Session = session;
		this.m_Socket.on('data', (data:string) => {
			this.m_Session.onData(data);
		});
	}

	Send(buffer:string){
		this.m_Socket.write(buffer);
		//send a new line to split on
		this.m_Socket.write('\n');
	}

	Receive() : string {
		let data = this.m_Socket.read();
		return data;
	}

	close(){
		this.m_Socket.destroy();
	}
}