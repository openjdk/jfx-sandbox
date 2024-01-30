package com.sun.glass.ui.headless;

import com.sun.glass.ui.Application;
import java.util.LinkedList;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.LinkedBlockingQueue;

public class NestedRunnableProcessor implements Runnable {

    private final LinkedList<RunLoopEntry> activeRunLoops = new LinkedList<>();

    private final BlockingQueue<Runnable> runnableQueue = new LinkedBlockingQueue<>();

    public void run() {
        newRunLoop();
    }

    void invokeLater(final Runnable r) {
        runnableQueue.add(r);
    }

    void runLater(Runnable r) {
        invokeLater(r);
    }

    void invokeAndWait(final Runnable r) {
        final CountDownLatch latch = new CountDownLatch(1);
        runnableQueue.add(() -> {
            try {
                r.run();
            } finally {
                latch.countDown();
            }
        });
        try {
            latch.await();
        } catch (InterruptedException e) {
        }
    }

    public Object newRunLoop() {
        RunLoopEntry entry = new RunLoopEntry();

        activeRunLoops.push(entry);

        entry.active = true;
        while (entry.active) {
            try {
                runnableQueue.take().run();
            } catch (Throwable e) {
                Application.reportException(e);
            }
        }

        return entry.returnValue;
    }

    private static class RunLoopEntry {

        boolean active;
        Object returnValue;
    }
}
