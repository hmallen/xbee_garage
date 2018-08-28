import datetime


if __name__ == '__main__':
    dt_current = datetime.datetime.now()
    time_message = dt_current.strftime('#m%md%dy%YH%HM%MS%S#')
    print(time_message)
