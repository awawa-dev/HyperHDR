/* Macros.cpp
*
*  MIT License
*
*  Copyright (c) 2020-2026 awawa-dev
*
*  Project homesite: https://github.com/awawa-dev/HyperHDR
*
*  Permission is hereby granted, free of charge, to any person obtaining a copy
*  of this software and associated documentation files (the "Software"), to deal
*  in the Software without restriction, including without limitation the rights
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is
*  furnished to do so, subject to the following conditions:
*
*  The above copyright notice and this permission notice shall be included in all
*  copies or substantial portions of the Software.

*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*  SOFTWARE.
*/

#include <utils/Macros.h>

void hyperhdr::THREAD_REMOVER(QString message, QThread* parentThread, QObject* client)
{
	SMARTPOINTER_MESSAGE(message);
	if (parentThread->isRunning())
	{
		if (client != nullptr)
			client->deleteLater();
		parentThread->quit();
		parentThread->wait();
	}
	else if (client != nullptr)
		delete client;
	delete parentThread;
}

void hyperhdr::THREAD_MULTI_REMOVER(QString message, QThread* parentThread, std::vector<QObject*> clients)
{	
	SMARTPOINTER_MESSAGE(message);
	if (parentThread->isRunning())
	{
		for(QObject*& client : clients)
			client->deleteLater();
		parentThread->quit();
		parentThread->wait();
	}
	else for (QObject*& client : clients)
		delete client;
	delete parentThread;
}

void hyperhdr::SMARTPOINTER_MESSAGE(QString message)
{
	auto messageChar = message.toUtf8();
	printf("SmartPointer is removing: %s\n", messageChar.constData());
}
