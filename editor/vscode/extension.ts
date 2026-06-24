import * as vscode from 'vscode';
import { LanguageClient, LanguageClientOptions,
         ServerOptions } from 'vscode-languageclient/node';

let client: LanguageClient;

export function activate(ctx: vscode.ExtensionContext) {
    const serverOptions: ServerOptions = {
        command: 'saqut',
        args: ['lsp']
    };
    const clientOptions: LanguageClientOptions = {
        documentSelector: [{ scheme: 'file', language: 'sqt' }]
    };
    client = new LanguageClient('saQut', 'saQut Language Server',
                                 serverOptions, clientOptions);

    const debugFactory: vscode.DebugAdapterDescriptorFactory = {
        createDebugAdapterDescriptor(_session) {
            return new vscode.DebugAdapterExecutable('saqut', ['dap']);
        }
    };
    ctx.subscriptions.push(client.start());
    ctx.subscriptions.push(
        vscode.debug.registerDebugAdapterDescriptorFactory('sqt', debugFactory)
    );
}

export function deactivate(): Thenable<void> | undefined {
    return client?.stop();
}
