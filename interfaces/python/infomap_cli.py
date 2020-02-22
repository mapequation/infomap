def main():
    import sys
    import infomap as im
    args = " ".join(sys.argv[1:])
    infomap = im.Infomap(im.Config(args, True))
    infomap.run()


if __name__ == '__main__':
    main()
