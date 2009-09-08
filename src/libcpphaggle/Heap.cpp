/* Copyright 2008-2009 Uppsala University
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <string.h>
#include <stdio.h>

#include <libcpphaggle/Heap.h>

namespace haggle {

void Heap::heapify(unsigned int i)
{
	unsigned int l, r, smallest;
	HeapItem *tmp;

	l = (2 * i) + 1;	/*left child */
	r = l + 1;		/*right child */

	if ((l < size) && (heap[l]->metric < heap[i]->metric))
		smallest = l;
	else
		smallest = i;

	if ((r < size) && (heap[r]->metric < heap[smallest]->metric))
		smallest = r;

	if (smallest == i)
		return;

	/* exchange to maintain heap property */
	tmp = heap[smallest];
	heap[smallest] = heap[i];
	heap[smallest]->index = smallest;
	heap[i] = tmp;
	heap[i]->index = i;
	heapify(smallest);
}

int Heap::increaseSize(unsigned int increase_size)
{
	HeapItem **new_heap;

	new_heap = new HeapItem *[max_size + increase_size];

	if (!new_heap) {
		return -1;
	}

	memcpy(new_heap, heap, size * sizeof(HeapItem *));

	delete[] heap;

	heap = new_heap;

	max_size += increase_size;

	return max_size;
}

int Heap::insert(HeapItem * item)
{
	unsigned int i, parent;

	if (isFull()) {
		if (increaseSize() < 0) {
			fprintf(stderr, "Heap is full and could not increase heap size, size=%d\n", size);
			return -1;
		}
	}

	i = size;
	parent = (i - 1) / 2;

	/*find the correct place to insert */
	while ((i > 0) && (heap[parent]->metric > item->metric)) {
		heap[i] = heap[parent];
		heap[i]->index = i;
		i = parent;
		parent = (i - 1) / 2;
	}
	heap[i] = item;
	item->index = i;
	size++;

	return 0;
}

HeapItem *Heap::extractFirst(void)
{
	HeapItem *max;

	if (isEmpty())
		return NULL;

	max = heap[0];
	size--;
	heap[0] = heap[size];
	heap[0]->index = 0;
	heapify(0);

	return max;
}

}; // namespace haggle
