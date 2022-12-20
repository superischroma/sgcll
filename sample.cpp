class point
{
public:
    int x;
    int y;

    point(int x, int y)
    {
        this->x = x;
        this->y = y;
    }

    point* operator+(int rhs)
    {
        point* p = new point(this->x, this->y);
        p->x += rhs;
        p->y += rhs;
        return p;
    }
};

int main()
{
    point p = point(5, 3);
    point* np = p + 5;
    point* np2 = 5 + p;
    delete np;
    delete np2;
}