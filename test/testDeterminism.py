import infomap
import sys
import argparse

def run(input, count):

    firstCodelength = 0.0

    for i in range(count):
        # infomapWrapper = infomap.Infomap("--silent")
        infomapWrapper = infomap.Infomap("-z")

        infomapWrapper.readInputData(input)

        infomapWrapper.run()

        tree = infomapWrapper.tree

        print("{}: Found {} top modules with codelength: {}".format(i, tree.numTopModules(), tree.codelength()))
        if i == 0:
            firstCodelength = tree.codelength()
        elif tree.codelength() != firstCodelength:
            print("Found non-deterministic behavior!")
            return 1

    print("Done! No non-deterministic behaviour found!")
    return 0


def main(argv):
    parser = argparse.ArgumentParser(description='Test determinism.')
    parser.add_argument('input', nargs='?', default='../ninetriangles.net', help='input network (default: ninetriangles.net)')
    parser.add_argument('-c', '--count', type=int, default=50, help='max count of iterations (deafult: 40)')

    args = parser.parse_args()
    return run(args.input, args.count)

if __name__ == "__main__":
   main(sys.argv[1:])