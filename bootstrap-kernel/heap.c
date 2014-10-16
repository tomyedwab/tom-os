// Dead simple heap allocator returns whole 4k pages

int current_page;

void initHeap() {
    // Start a this page and keep handing out pages going up the address space
    current_page = 0x100000;
}

void *allocPage() {
    int page = current_page;
    current_page += 0x1000;
    return (void *)page;
}
