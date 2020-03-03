def main():
    import sys
    import infomap as im
    args = " ".join(sys.argv[1:])
    conf = im.Config(args, True)
    infomap = im.Infomap(conf)
    infomap.run()


if __name__ == '__main__':
    main()
