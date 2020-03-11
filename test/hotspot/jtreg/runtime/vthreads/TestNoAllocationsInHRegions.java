/*
 * Copyright (c) 2016, 2020, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

import jdk.test.lib.Utils;
import java.util.LinkedList;
import java.util.List;
import java.util.Random;
import java.util.stream.Collectors;

/**
 * @test TestNoAllocationsInHRegions
 * @summary Stress test of allocation in vthreads
 * @library /test/lib /
 * @modules java.management java.base/jdk.internal.misc
 *
 * @run main/othervm TestNoAllocationsInHRegions 20 70
 */
public class TestNoAllocationsInHRegions {
    private static final Random RND = Utils.getRandomInstance();
    private static final int REGION_SIZE = 1024 * 1024;  //WB.g1RegionSize();
    private static final int[] HUMONGOUS_SIZES = {REGION_SIZE / 2, REGION_SIZE + 1, REGION_SIZE * 2 + 1};
    private static final int ALLOC_THREAD_COUNT = 5;

    // We fill specified part of heap with humongous objects - we need public static to prevent escape analysis to
    // collect this field
    public static LinkedList<byte[]> humongousAllocations = new LinkedList<>();

    private static volatile boolean shouldStop = false;
    private static volatile Error error = null;

    static class Allocator implements Runnable {

        private final List<byte[]> liveObjects = new LinkedList<>();
        private int usedMemory = 0;
        public static volatile Object blackHole;
        public final Runnable[] actions;

        /**
         * Maximum size of simple allocation
         */
        private static final int MAX_ALLOCATION_SIZE = (int) (REGION_SIZE / 2 * 0.9);

        /**
         * Maximum size of dead (i.e. one which is made unreachable right after allocation) object
         */
        private static final int DEAD_OBJECT_MAX_SIZE = REGION_SIZE / 10;

        public Allocator(int maxAllocationMemory) {

            actions = new Runnable[]{
                    // Allocation
                    () -> {
                        if (maxAllocationMemory - usedMemory != 0) {
                            int arraySize = RND.nextInt(Math.min(maxAllocationMemory - usedMemory,
                                    MAX_ALLOCATION_SIZE));

                            if (arraySize != 0) {
                                byte[] allocation = new byte[arraySize];
                                liveObjects.add(allocation);
                                usedMemory += arraySize;
                            }
                        }
                    },

                    // Deallocation
                    () -> {
                        if (liveObjects.size() != 0) {
                            int elementNum = RND.nextInt(liveObjects.size());
                            int shouldFree = liveObjects.get(elementNum).length;
                            liveObjects.remove(elementNum);
                            usedMemory -= shouldFree;
                        }
                    },

                    // Dead object allocation
                    () -> {
                        int size = RND.nextInt(DEAD_OBJECT_MAX_SIZE);
                        blackHole = new byte[size];
                    }
            };
        }

        @Override
        public void run() {
            while (!shouldStop) {
                actions[RND.nextInt(actions.length)].run();
                Thread.yield();
            }
        }
    }

    public static void main(String[] args) {
        if (args.length != 2) {
            throw new Error("Test Bug: Expected duration (in seconds) and percent of allocated regions were not "
                    + "provided as command line argument");
        }

        // test duration
        long duration = Integer.parseInt(args[0]) * 1000L;
        // part of heap preallocated with humongous objects (in percents)
        int percentOfAllocatedHeap = Integer.parseInt(args[1]);

        long startTime = System.currentTimeMillis();

        long initialFreeRegionsCount = Runtime.getRuntime().freeMemory() / REGION_SIZE ;// WB.g1NumFreeRegions();
        int regionsToAllocate = (int) ((double) initialFreeRegionsCount / 100.0 * percentOfAllocatedHeap);
        long freeRegionLeft = initialFreeRegionsCount - regionsToAllocate;

        System.out.println("Regions to allocate: " + regionsToAllocate + "; regions to left free: " + freeRegionLeft);

        int maxMemoryPerAllocThread = (int) ((Runtime.getRuntime().freeMemory() / 100.0
                * (100 - percentOfAllocatedHeap)) / ALLOC_THREAD_COUNT * 0.5);

        System.out.println("Using " + maxMemoryPerAllocThread / 1024 + "KB for each of " + ALLOC_THREAD_COUNT
                + " allocation threads");

        //        while (WB.g1NumFreeRegions() > freeRegionLeft) {
        // while ((Runtime.getRuntime().freeMemory() / Runtime.getRuntime().maxMemory()) < 100 - percentOfAllocatedHeap) {
        while(true) {
            try {
                humongousAllocations.add(new byte[HUMONGOUS_SIZES[RND.nextInt(HUMONGOUS_SIZES.length)]]);
            } catch (OutOfMemoryError oom) {
                //We got OOM trying to fill heap with humongous objects
                //It probably means that heap is fragmented which is strange since the test logic should avoid it
                System.out.println("Warning: OOM while allocating humongous objects - it likely means "
                        + "that heap is fragmented");
                break;
            }
        }

        LinkedList<Thread> threads = new LinkedList<>();

        for (int i = 0; i < ALLOC_THREAD_COUNT; i++) {
            threads.add(Thread.builder().task(new Allocator(maxMemoryPerAllocThread)).virtual().build());
        }

        threads.stream().forEach(Thread::start);

        while ((System.currentTimeMillis() - startTime < duration) && error == null) {
            Thread.yield();
        }

        shouldStop = true;
        System.out.println("Finished test");
        if (error != null) {
            throw error;
        }
    }
}
