// EDGE: pointer aliasing, self-referential structs, linked lists,
// pointer-to-pointer, NULL handling.

// two pointers aliasing the same storage
I64 x=10;
I64 *p=&x,*q=&x;
*p=*p+5;
"%d %d\n",*q,x;              // 15 15
*q=100;
"%d\n",*p;                  // 100

// pointer to pointer
I64 **pp=&p;
**pp=42;
"%d\n",x;                    // 42
"%d\n",*p==x;              // 1 (p still aliases x)

// self-referential struct: a singly linked list built on the heap
class Node
{
  Node *next;
  I64 val;
};

Node *head=NULL;
I64 i;
for (i=5;i>=1;i--) {         // push 5,4,3,2,1 -> list is 1,2,3,4,5
  Node *n=MAlloc(sizeof(Node));
  n->next=head;
  n->val=i;
  head=n;
}
// walk & sum
I64 sum=0;
Node *cur=head;
while (cur) {
  "%d ",cur->val;
  sum+=cur->val;
  cur=cur->next;
}
'\n';
"%d\n",sum;                  // 15

// reverse the list in place (classic pointer surgery). HolyC locals are
// NoDups, so declare the cursors once and reuse them.
Node *prev=NULL,*nxt=NULL;
cur=head;
while (cur) {
  nxt=cur->next;
  cur->next=prev;
  prev=cur;
  cur=nxt;
}
head=prev;
for (cur=head;cur;cur=cur->next) "%d ",cur->val;
'\n';                        // 5 4 3 2 1

// free the list (Free(NULL) at the end is fine)
cur=head;
while (cur) {
  nxt=cur->next;
  Free(cur);
  cur=nxt;
}
Free(cur);                   // NULL

// self-referential via array of pointers + aliasing cycle
Node a,b;
a.val=1; b.val=2;
a.next=&b;
b.next=&a;                   // 2-cycle
"%d %d %d\n",a.next->val,a.next->next->val,a.next->next->next->val;  // 2 1 2

// pointer arithmetic aliasing across an array
I64 arr[5];
for (i=0;i<5;i++) arr[i]=i*i;
I64 *base=arr;
I64 *mid=&arr[2];
"%d %d\n",*mid,mid[-1];     // 4 1  (negative index scales back)
"%d\n",mid-base;            // 16 bytes between them (unscaled diff)
mid[0]=99;
"%d\n",arr[2];              // 99 (aliased write)

// NULL-safety patterns
Node *z=NULL;
"%d\n",z==NULL;            // 1
if (!z) "null ok\n";
