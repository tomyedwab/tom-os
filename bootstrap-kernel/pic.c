void initPIC() {
    // ICW1 to primary PIC
    outb(0x20, 0x11);

    // ICW1 to slave PIC
    outb(0xA0, 0x11);

    // ICW2: Map primary PIC IRQs to interrupts 32-39
    outb(0x21, 0x20);

    // ICW2: Map slave PIC IRQs to interrupts 40-47
    outb(0xA1, 0x28);

    // ICW3: Set cascading primary on IRQ2
    outb(0x21, 0x4); // 2nd bit signifies IRQ2

    // ICW3: Set cascading slave on IRQ2
    outb(0xA1, 0x2); // 2 in binary signifies IRQ2

    // ICW4 to primary PIC
    outb(0x21, 0x1);

    // ICW4 to slave PIC
    outb(0xA1, 0x1);
}
