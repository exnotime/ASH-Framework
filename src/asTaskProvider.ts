import * as path from 'path';
import * as fs from 'fs';
import * as cp from 'child_process';
import * as vscode from 'vscode';

export class ASTaskProvider implements vscode.TaskProvider {
	static asType: string = 'ash_build';
	private asPromise: Thenable<vscode.Task[]> | undefined = undefined;

	constructor(workspaceRoot: string) {
		let pattern = path.join(workspaceRoot, 'AngelScript');
		let fileWatcher = vscode.workspace.createFileSystemWatcher(pattern);
		fileWatcher.onDidChange(() => this.asPromise = undefined);
		fileWatcher.onDidCreate(() => this.asPromise = undefined);
		fileWatcher.onDidDelete(() => this.asPromise = undefined);
	}

	public provideTasks(): Thenable<vscode.Task[]> | undefined {
		if (!this.asPromise) {
			this.asPromise = getASTasks();
		}
		return this.asPromise;
	}

	public resolveTask(_task: vscode.Task): vscode.Task | undefined {
		const task = _task.definition.task;
		if (task) {
			// resolveTask requires that the same definition object be used.
			const definition: ASTaskDefinition = <any>_task.definition;
			return new vscode.Task(definition, definition.task, 'AngelScript', new vscode.ShellExecution(`AngelScript ${definition.task}`));
		}
		return undefined;
	}
}

function exists(file: string): Promise<boolean> {
	return new Promise<boolean>((resolve, _reject) => {
		fs.exists(file, (value) => {
			resolve(value);
		});
	});
}

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

let _channel: vscode.OutputChannel;
function getOutputChannel(): vscode.OutputChannel {
	if (!_channel) {
		_channel = vscode.window.createOutputChannel('AngelScript builder');
	}
	return _channel;
}

interface ASTaskDefinition extends vscode.TaskDefinition {
}

const buildNames: string[] = ['build', 'compile', 'watch'];
function isBuildTask(name: string): boolean {
	for (let buildName of buildNames) {
		if (name.indexOf(buildName) !== -1) {
			return true;
		}
	}
	return false;
}

const testNames: string[] = ['test'];
function isTestTask(name: string): boolean {
	for (let testName of testNames) {
		if (name.indexOf(testName) !== -1) {
			return true;
		}
	}
	return false;
}

async function getASTasks(): Promise<vscode.Task[]> {
	let emptyTasks: vscode.Task[] = [];
	let workspaceFolders = vscode.workspace.workspaceFolders;
	if(workspaceFolders === undefined){
		return emptyTasks;
	}
	let workspaceRoot = workspaceFolders[0];
	if (!workspaceRoot) {
		return emptyTasks;
	}
	let settings = vscode.workspace.getConfiguration('angelscript_helper');
    let interfaceFile = settings['interfaceLocation'];
    if(interfaceFile === ''){
        return emptyTasks;
    }
    let ashLocation = settings['ashExeLocation'];
    if(ashLocation === ''){
        return emptyTasks;
    }
    let sourceFile = settings['sourceFile'];
    if(sourceFile === ''){
        return emptyTasks;
    }
	let commandLine = ashLocation + ' -c ' + sourceFile + ' -i ' + interfaceFile;
	let result: vscode.Task[] = [];
	let kind : ASTaskDefinition = {
		type: ASTaskProvider.asType
	};
	let buildTask = new vscode.Task(kind, workspaceRoot, 'ash_build', 'ASH', new vscode.ShellExecution(commandLine));
	result.push(buildTask);
	return result;
}